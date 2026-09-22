const searchButton = document.getElementById("searchButton");

searchButton.addEventListener("click", function () {

    const day = document.getElementById("day").value;
    const startTime = document.getElementById("startTime").value;
    const endTime = document.getElementById("endTime").value;

    if (day === "" || startTime === "" || endTime === "") {
        alert("Please select the day, start time and end time.");
        return;
    }

    document.getElementById("roomResults").innerHTML =
        "<p>Searching for available rooms...</p>";
});