package main

import (
	"encoding/csv"
	"fmt"
	"net/http"
	"os"
	"time"
)

func main() {

	http.HandleFunc("/simulate", func(w http.ResponseWriter, r *http.Request) {

		filePath := "backend/attacks.csv"

		file, err := os.OpenFile(
			filePath,
			os.O_APPEND|os.O_CREATE|os.O_WRONLY,
			0644,
		)
		if err != nil {
			fmt.Println("File error:", err)
			return
		}
		defer file.Close()

		writer := csv.NewWriter(file)

		// ✅ write headers only if file is empty
		info, _ := file.Stat()
		if info.Size() == 0 {
			writer.Write([]string{"ip", "time", "reason", "port", "country"})
		}

		writer.Write([]string{
			"185.220.101.5",
			time.Now().String(),
			"SSH Brute Force",
			"22",
			"RU",
		})

		writer.Flush()

		fmt.Fprintf(w, "Attack Logged")
	})

	fmt.Println("Server running on port 8080")
	http.ListenAndServe(":8080", nil)
}
