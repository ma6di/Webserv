// Top-level navigations so the server's error pages render in the browser.
// 405/413 use real <form> submissions; 500/504 use window.location.

(() => {


  document.addEventListener("DOMContentLoaded", () => {
    trigger405();
    trigger413();
    // waitingAnimation()
  });



  // ---- 405: posts to a get only route in /scripts cgi -----------------------------------
  function trigger405() {
    const btn = document.querySelector(".slide-three .trigger-btn");
    if (!btn) return;

    // Creates a hidden form 
    const form = document.createElement("form");
    form.method = "POST";
    form.action = "/scripts"; // post method not allowed here
    form.style.display = "none";
    document.body.appendChild(form);
    btn.addEventListener("click", () => {
    //   busy(btn, true);
      // normal form POST -> browser navigates -> server should return 405 page
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
    // default enctype is fine; body size is what matters for 413
    form.style.display = "none";

    // Big payload (~1.2MB). Adjust if your client_max_body_size differs.
    const ta = document.createElement("textarea");
    ta.name = "payload";
    ta.value = "A".repeat(1200 * 1024); // ~1.2MB of 'A'
    form.appendChild(ta);

    document.body.appendChild(form);

    btn.addEventListener("click", () => {
    //   busy(btn, true);
      form.submit(); // browser navigates; your server should respond 413 + error page
    });
  }

//   function waitingAnimation() {
//   const btn = document.querySelector(".slide-six .trigger-btn");
//   if (!btn) return;

//   btn.addEventListener("click", (e) => {
//     e.preventDefault();

//     // Disable button + start "Waiting…" animation
//     btn.disabled = true;
//     let dots = 0;
//     const interval = setInterval(() => {
//       dots = (dots + 1) % 4; // cycles 0–3
//       btn.textContent = "Waiting" + ".".repeat(dots);
//     }, 500);

//     // Navigate to the slow CGI endpoint that should timeout
//     window.location.href = "/cgi-bin/trigger_504.py";

//     // Optional fallback: clear animation after ~30s if navigation fails
//     setTimeout(() => clearInterval(interval), 30000);
//   });
// }

})();
