function showModal() {
    const modal = new bootstrap.Modal(
        document.getElementById("uploadModal")
    );
    modal.show();
}

document.getElementById("modalFileInput").addEventListener("change", function () {
    const file = this.files[0];
    const readyNotice = document.getElementById("fileReady");
    const nameInput = document.getElementById("photoName");

    if (!file) {
        readyNotice.classList.add("d-none");
        nameInput.value = "";
        return;
    }

    const allowedTypes = ["image/jpeg", "image/png", "image/webp", "image/gif"];
    if (!allowedTypes.includes(file.type)) {
        alert("❌ Only JPG, PNG, WEBP, and GIF files are allowed.");
        this.value = "";
        readyNotice.classList.add("d-none");
        nameInput.value = "";
        return;
    }

    readyNotice.classList.remove("d-none");

    // sets value to filename without extension
    var baseName = file.name.replace(/\.[^/.]+$/, "");
    nameInput.value = baseName || "";
});


//POST REQUEST
document.getElementById("photoUploadForm").addEventListener("submit", async (e) => {
    e.preventDefault();

    const fileInput = document.getElementById("modalFileInput");
    const nameInput = document.getElementById("photoName");
    const file = fileInput.files[0];
    if (!file) return;

    // sanitizes base name from user input or fallbacks to original
    const rawBase = (nameInput.value || file.name.replace(/\.[^/.]+$/, "")).trim();
    const safeBase = rawBase.replace(/[^a-zA-Z0-9-_ ]+/g, "").replace(/\s+/g, "-") || "photo";

    // keep original extension
    const ext = (file.name.match(/\.[^.]+$/) || [""])[0].toLowerCase();
    const renamed = new File([file], `${safeBase}${ext}`, { type: file.type });

    // build form data and override the file field's filename
    const form = e.target;
    const formData = new FormData(form);
    formData.set("file", renamed);

    try {
        const res = await fetch("/upload", {
            method: "POST",
            headers: { "X-Frontend": "1", "Accept": "application/json" },
            body: formData,
        });
        if (!res.ok) throw new Error("POST failed: " + res.status);
        const data = await res.json();

        console.log("✅ POST request successful, uploaded the following file: " + data.path)
        alert("Upload successful: " + data.path);
        form.reset();
        document.getElementById("fileReady").classList.add("d-none");
        bootstrap.Modal.getInstance(document.getElementById("uploadModal")).hide();
        location.reload(); // or refresh() your gallery
    } catch (err) {
        console.error("❌ Upload error:", err);
        alert("Upload failed");
    }
});


function isImageFile(name) {
    return /\.(jpe?g|png|gif|webp)$/i.test(name);
}

//GET REQUEST
fetch("/upload", {
    headers: { "X-Frontend": "1", "Accept": "application/json" }
})
    .then((res) => {
        if (!res.ok) throw new Error("HTTP error " + res.status);
        return res.json();// { ok:true, files:[...]}
    })
    .then(({ files = [] }) => {
        console.log("✅ GET request handled successfully!");
        console.log("Returned array from upload folder:", files);
        const data = files.filter(isImageFile);
        // makes the "empty photobooth" warning visible or not
        if (!Array.isArray(data) || data.length === 0) {
            document
                .getElementById("emptyMessage")
                .classList.remove("d-none");
            return;
        } else {
            document
                .getElementById("emptyMessage")
                .classList.add("d-none");
        }
        const grid = document.querySelector(".photo-grid");
        let fileToDelete = null;
        let cardToDelete = null;
        document.getElementById("confirmDeleteBtn").onclick =
            () => {
                if (!fileToDelete || !cardToDelete) return;

                fetch(`/upload/${fileToDelete}`, {
                    method: "DELETE",
                })
                    .then((res) => {
                        if (!res.ok)
                            throw new Error("Delete failed");

                        cardToDelete.remove();

                        const toast = new bootstrap.Toast(
                            document.getElementById("deleteToast")
                        );
                        toast.show();
                        storedeleteFileName = fileToDelete
                        fileToDelete = null;
                        cardToDelete = null;

                        bootstrap.Modal.getInstance(
                            document.getElementById(
                                "confirmDeleteModal"
                            )
                        ).hide();
                        // shows "empty photobook" message
                        if (
                            document.querySelectorAll(".photo-card")
                                .length === 1
                        ) {
                            // Only the upload button shows if empty
                            document
                                .getElementById("emptyMessage")
                                .classList.remove("d-none");
                        }
                        console.log("✅ DELETE request handled successfully!");
                        console.log("Deleted file: upload/" + storedeleteFileName);
                    })
                    .catch((err) => {
                        console.error(
                            "❌ Could not delete image:",
                            err
                        );
                        alert("Error: failed to delete image.");
                    });
            };

        data.forEach((filename) => {
            const card = document.createElement("div");
            card.className = "photo-card";
            // Creates the spinner on each loading photo
            const spinner = document.createElement("div");
            spinner.className = "spinner-border text-secondary";
            spinner.setAttribute("role", "status");
            card.appendChild(spinner);

            // Creates the image element
            const img = document.createElement("img");
            img.src = `/upload/${filename}`;
            img.alt = filename;
            img.style.display = "none"; // Hide until loaded
            const loadDelay = 800; // ms

            let finished = false;

            function revealImage(success) {
                if (finished) return;
                finished = true;

                setTimeout(() => {
                    spinner.remove();

                    if (success) {
                        img.style.display = "block";
                        img.classList.add("visible");
                    } else {
                        const errorText =
                            document.createElement("div");
                        errorText.textContent = "❌ Failed to load";
                        errorText.style.fontSize = "0.9rem";
                        errorText.style.color = "#dc3545";
                        card.appendChild(errorText);
                    }
                }, loadDelay);
            }

            img.onload = () => revealImage(true);
            img.onerror = () => revealImage(false);
            card.appendChild(img);
            grid.appendChild(card);

            // DELETE BUTTON AND REQUEST
            const deleteBtn = document.createElement("button");
            deleteBtn.textContent = "×";
            deleteBtn.className = "delete-btn";
            deleteBtn.title = "Delete photo";

            // PHASE 1: When user clicks X button
            deleteBtn.onclick = () => {
                fileToDelete = filename;
                cardToDelete = card;

                const modal = new bootstrap.Modal(
                    document.getElementById("confirmDeleteModal")
                );
                modal.show();
            };

            card.appendChild(deleteBtn);
        });
    })
    .catch((err) => {
        console.log("❌ Fetch failed!");
        console.error("Request returned:", err);
    });