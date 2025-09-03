(() => {
  document.addEventListener("DOMContentLoaded", () => {
    trigger405();
    trigger413();
  });

  // ---- 405: posts to a get only route in /scripts cgi -----------------------------------
  function trigger405() {
    const btn = document.querySelector(".slide-three .trigger-btn");
    if (!btn) return;
    // Creates a hidden form
    const form = document.createElement("form");
    form.method = "POST";
    form.action = "/scripts"; // post method not allowed here, therefore errror triggered
    form.style.display = "none";
    document.body.appendChild(form);
    btn.addEventListener("click", () => {
      form.submit();
    });
  }

  // ---- 413: POST body larger than limit --------------------------------
  function trigger413() {
    const btn = document.querySelector(".slide-four .trigger-btn");
    if (!btn) return;

    // We’ll use application/x-www-form-urlencoded with a huge textarea value.
    // This triggers 413 at the server level (before/while parsing).
    const form = document.createElement("form");
    form.method = "POST";
    form.action = "/upload";
    form.style.display = "none";
    // ~1.2MB payload which is bigger than our max body size
    const ta = document.createElement("textarea");
    ta.name = "payload";
    ta.value = "A".repeat(1200 * 1024); // ~1.2MB of 'A'
    form.appendChild(ta);
    document.body.appendChild(form);
    btn.addEventListener("click", () => {
      form.submit(); // browser navigates through post method and end with payload too large
    });
  }
})();
