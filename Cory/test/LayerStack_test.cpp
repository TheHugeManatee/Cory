#include <catch2/catch_test_macros.hpp>

#include <Cory/Application/ApplicationLayer.hpp>
#include <Cory/Application/LayerStack.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>

#include "TestUtils.hpp"

class MockLayer : public Cory::ApplicationLayer {
  public:
    MockLayer(std::string name, int &counter)
        : ApplicationLayer(name)
        , counter(counter)
    {
    }
    ~MockLayer() { REQUIRE(detached_); }

    int &counter;
    bool attached_{false};
    bool detached_{false};
    int updatedIndex_{-1};
    bool hasRenderTask_{false};
    bool acceptsEvents_{false};
    std::vector<Cory::Event> receivedEvents_;

    bool hasRenderTask() const override { return hasRenderTask_; }
    void onAttach(Cory::Context &ctx, Cory::LayerAttachInfo info) override { attached_ = true; }
    void onDetach(Cory::Context &ctx) override { detached_ = true; }
    void onUpdate() override { updatedIndex_ = counter++; }

    bool onEvent(Cory::Event event) override
    {
        updatedIndex_ = counter++;
        if (acceptsEvents_) {
            receivedEvents_.push_back(event);
            return true;
        }
        return false;
    }

    Cory::RenderTaskDeclaration<Cory::LayerPassOutputs>
    renderTask(Cory::RenderTaskBuilder builder, Cory::LayerPassOutputs previousLayer) override
    {
        updatedIndex_ = counter++;
        co_yield Cory::LayerPassOutputs{};
    }
};

TEST_CASE("LayerStack", "[LayerStack]")
{
    Cory::testing::VulkanTester tester;

    GIVEN("A layer stack")
    {
        Cory::LayerStack stack(tester.ctx());
        WHEN("Attaching layers")
        {
            int counter{0};
            Cory::LayerAttachInfo info{};
            MockLayer &layer1 = stack.addLayer<MockLayer>(info, "Layer 1", std::ref(counter));
            MockLayer &layer2 = stack.addLayer<MockLayer>(info, "Layer 2", std::ref(counter));
            MockLayer &layer3 = stack.addLayer<MockLayer>(info, "Layer 3", std::ref(counter));

            REQUIRE(stack.layers().size() == 3);

            THEN("The layers are initialized correctly")
            {
                REQUIRE(layer1.name.get() == "Layer 1");
                REQUIRE(layer2.name.get() == "Layer 2");
                REQUIRE(layer3.name.get() == "Layer 3");
            }

            THEN("The layers are attached correctly")
            {
                REQUIRE(layer1.attached_);
                REQUIRE(layer2.attached_);
                REQUIRE(layer3.attached_);
            }

            AND_WHEN("Removing a layer")
            {
                auto layer_ptr = stack.removeLayer("Layer 2");
                THEN("The layer is not in the stack anymore")
                {
                    REQUIRE(stack.layers().size() == 2);
                    REQUIRE(stack.layers()[0].get() == &layer1);
                    REQUIRE(stack.layers()[1].get() == &layer3);
                }
                THEN("The layer is detached")
                {
                    REQUIRE(static_cast<MockLayer *>(layer_ptr.get())->detached_);
                }
            }
        }
    }
    GIVEN("A layer stack with layers")
    {
        Cory::LayerStack stack(tester.ctx());

        int counter{0};
        Cory::LayerAttachInfo info{};
        MockLayer &layer1 = stack.addLayer<MockLayer>(info, "Layer 1", std::ref(counter));
        MockLayer &layer2 = stack.addLayer<MockLayer>(info, "Layer 2", std::ref(counter));
        MockLayer &layer3 = stack.addLayer<MockLayer>(info, "Layer 3", std::ref(counter));

        WHEN("Updating the stack")
        {
            stack.update();
            THEN("The layers are updated in the correct order")
            {
                REQUIRE(layer1.updatedIndex_ == 0);
                REQUIRE(layer2.updatedIndex_ == 1);
                REQUIRE(layer3.updatedIndex_ == 2);
            }
        }
        WHEN("Passing events through a stack that does not accept any events")
        {
            bool processed = stack.onEvent(Cory::Event{});
            THEN("The event is passed to the layers that accept it")
            {
                REQUIRE(layer1.receivedEvents_.size() == 0);
                REQUIRE(layer2.receivedEvents_.size() == 0);
                REQUIRE(layer3.receivedEvents_.size() == 0);

                REQUIRE(layer1.updatedIndex_ == 2);
                REQUIRE(layer2.updatedIndex_ == 1);
                REQUIRE(layer3.updatedIndex_ == 0);

                REQUIRE(processed == false);
            }
        }
        WHEN("Passing events down a stack where at least one layer accepts it")
        {
            layer1.acceptsEvents_ = true;
            layer2.acceptsEvents_ = true;
            bool processed = stack.onEvent(Cory::Event{});
            THEN("The event is passed to the first layer from the top that accepts it")
            {
                REQUIRE(layer1.receivedEvents_.size() == 0);
                REQUIRE(layer2.receivedEvents_.size() == 1);
                REQUIRE(layer3.receivedEvents_.size() == 0);

                REQUIRE(processed == true);
            }
        }
        WHEN("Enqueueing the render tasks")
        {
            Cory::Framegraph framegraph(tester.ctx());

            SECTION("No layers have a render task")
            {
                stack.declareRenderTasks(framegraph, Cory::LayerPassOutputs{});
                THEN("No layer is called")
                {
                    REQUIRE(layer1.updatedIndex_ == -1);
                    REQUIRE(layer2.updatedIndex_ == -1);
                    REQUIRE(layer3.updatedIndex_ == -1);
                }
            }

            SECTION("All layers have a render task")
            {
                layer1.hasRenderTask_ = true;
                layer2.hasRenderTask_ = true;
                layer3.hasRenderTask_ = true;
                stack.declareRenderTasks(framegraph, Cory::LayerPassOutputs{});
                THEN("The layers are updated in the correct order")
                {
                    REQUIRE(layer1.updatedIndex_ == 0);
                    REQUIRE(layer2.updatedIndex_ == 1);
                    REQUIRE(layer3.updatedIndex_ == 2);
                }
            }
        }
    }
}