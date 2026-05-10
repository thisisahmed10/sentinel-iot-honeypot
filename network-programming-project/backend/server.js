const express = require("express");
const fs = require("fs");

const app = express();
app.use(express.json());

let logs = [];

app.post("/log", (req, res) => {
    console.log("Attack:", req.body);

    logs.push({
        time: new Date(),
        ...req.body
    });

    fs.writeFileSync("logs.json", JSON.stringify(logs, null, 2));

    res.send("OK");
});

// optional: dashboard API
app.get("/logs", (req, res) => {
    res.json(logs);
});

app.listen(5000, () => {
    console.log("Backend running on port 5000");
});
