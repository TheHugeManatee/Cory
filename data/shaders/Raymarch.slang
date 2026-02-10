module Raymarch;

import Raycast;

public struct MarchConfig
{
    public float dt;
    public uint maxSteps;
};

public struct SampleRGBA
{
    public float3 rgb;
    public float a;
};

public interface ISampler<SampleT>
{
    public SampleT eval(Ray ray, float t);
}

public interface IAccumulator<SampleT, StateT, ResultT>
{
    void init(out StateT state, Ray ray, MarchConfig cfg);
    void step(inout StateT state, SampleT s, Ray ray, float t);
    bool shouldTerminate(StateT state);
    ResultT finalize(StateT state);
};

public ResultT raymarch<typename SampleT, typename StateT, typename ResultT, typename SamplerT, typename AccumT>(
    Ray ray,
    MarchConfig cfg,
    SamplerT sampler,
    AccumT accum)
    where SamplerT : ISampler<SampleT>
    where AccumT : IAccumulator<SampleT, StateT, ResultT>
{
    StateT st;
    accum.init(st, ray, cfg);

    if (cfg.dt <= 0.0f)
        return accum.finalize(st);

    for (uint step = 0u;; ++step) {
        if (cfg.maxSteps > 0u && step >= cfg.maxSteps)
            break;

        let t = ray.tMin + (float(step) + 0.5f) * cfg.dt;
        if (t > ray.tMax)
            break;

        let s = sampler.eval(ray, t);
        accum.step(st, s, ray, t);

        if (accum.shouldTerminate(st))
            break;
    }

    return accum.finalize(st);
}
