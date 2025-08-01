document.addEventListener('DOMContentLoaded', function () {
    console.log("Webserv static site loaded!");
});

/* Shows navbar when scrolling down */
let positionStart = 0;
const navbar = document.getElementById("mainNavbar");
window.addEventListener("scroll", function () {
    const positionOnScroll = window.pageYOffset || document.documentElement.positionOnScroll;
    if (positionOnScroll > positionStart) {
        navbar.style.top = "0px"; //
    } else {
        navbar.style.top = "-80px"; // hides the navbar when scrolling up
    }
    positionStart = positionOnScroll;
});

/* Shows special footer message when user arrives at end of page */
const footer = document.getElementById("endFooter");
window.addEventListener("scroll", function () {
    const scrollable = document.documentElement.scrollHeight - window.innerHeight;
    const scrolled = window.scrollY;
    if (scrolled >= scrollable - 5) {
        footer.classList.add("show");
    } else {
        footer.classList.remove("show");
    }
});

/* activate/deactivate nav link */
document.querySelectorAll('.nav-link').forEach(link => {
    link.addEventListener('click', function () {
        document.querySelectorAll('.nav-link').forEach(el => el.classList.remove('active'));
        this.classList.add('active');
    });
});

/* remove anchor hashes from html */
function scrollToSection(id) {
    const el = document.getElementById(id);
    if (el) {
        el.scrollIntoView({ behavior: "smooth" });
        history.replaceState(null, "", window.location.pathname); // ✅ clean URL with no #
    }
}

/* Redirection modal */
let redirectInterval = null; // global or module-scoped to access from cancel button
function redirectionCountdown() {
    const modal = new bootstrap.Modal(document.getElementById('redirectModal'));
    let countdownValue = 5;
    const countdownSpan = document.getElementById('countdown');

    modal.show();
    const interval = setInterval(() => {
        countdownValue--;
        countdownSpan.textContent = countdownValue;
        if (countdownValue === 0) {
            clearInterval(interval);
            window.location.href = "http://localhost:8080/42";
            modal.hide();
        }
    }, 1000);
}
