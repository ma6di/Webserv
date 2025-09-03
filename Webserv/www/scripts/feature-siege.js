(function () {
    const goals = {
        availability: (v) => v >= 99.5,
        response_time: (v) => v <= 0.3,
        failed_transactions: (v) => v === 0,
        longest_transaction: (v) => v <= 1.0,
    };
    document.querySelectorAll(".metric-card").forEach((card) => {
        const key = card.dataset.key;
        const raw = parseFloat(card.dataset.value);
        if (!goals[key]) return;
        const ok = goals[key](raw);
        card.classList.add(ok ? "good" : "bad");
        // add pass/fail badge
        const badge = document.createElement("span");
        badge.className =
            "badge metric-badge " +
            (ok ? "bg-success" : "bg-danger");
        badge.textContent = ok ? "PASS" : "FAIL";
        card.querySelector(".d-flex").appendChild(badge);
    });
})();