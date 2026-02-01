(() => {
  let vizPromise;

  function getViz() {
    // viz-global.js defines a global "Viz"
    if (!vizPromise) vizPromise = Viz.instance();
    return vizPromise;
  }

  async function renderAll() {
    const blocks = document.querySelectorAll("pre.dot");
    if (!blocks.length) return;

    const viz = await getViz();

    blocks.forEach((el) => {
      const dot = el.textContent;
      const out = document.createElement("div");
      out.className = "graphviz";
      el.replaceWith(out);

      try {
        out.appendChild(viz.renderSVGElement(dot));
      } catch (e) {
        out.textContent = String(e?.message ?? e);
      }
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", renderAll);
  } else {
    renderAll();
  }
})();