package main

import (
	"encoding/json"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

type Event struct {
	Time      string `json:"time"`
	SrcIP     string `json:"src_ip,omitempty"`
	SrcPort   uint16 `json:"src_port,omitempty"`
	DstPort   uint16 `json:"dst_port,omitempty"`
	Timestamp string `json:"timestamp,omitempty"`
	Payload   string `json:"payload,omitempty"`
	Type      string `json:"type,omitempty"`
	Message   string `json:"message,omitempty"`
}

type Store struct {
	mu       sync.Mutex
	logs     []Event
	path     string
	writeMu  sync.Mutex
	upgrader websocket.Upgrader
	clients  map[*websocket.Conn]struct{}
}

func newStore(path string) *Store {
	return &Store{
		path: path,
		upgrader: websocket.Upgrader{
			CheckOrigin: func(r *http.Request) bool { return true },
		},
		clients: make(map[*websocket.Conn]struct{}),
	}
}

func (s *Store) load() {
	data, err := os.ReadFile(s.path)
	if err != nil {
		if os.IsNotExist(err) {
			s.logs = []Event{}
			return
		}
		log.Printf("load logs: %v", err)
		s.logs = []Event{}
		return
	}

	if err := json.Unmarshal(data, &s.logs); err != nil {
		log.Printf("parse logs: %v", err)
		s.logs = []Event{}
	}
}

func (s *Store) persist() {
	data, err := json.MarshalIndent(s.logs, "", "  ")
	if err != nil {
		log.Printf("marshal logs: %v", err)
		return
	}

	if err := os.WriteFile(s.path, data, 0644); err != nil {
		log.Printf("write logs: %v", err)
	}
}

func (s *Store) appendEvent(event Event) Event {
	event.Time = time.Now().UTC().Format(time.RFC3339Nano)

	s.mu.Lock()
	defer s.mu.Unlock()

	s.logs = append(s.logs, event)
	s.persist()
	go s.broadcast(event)
	return event
}

func (s *Store) broadcast(event Event) {
	payload, err := json.Marshal(event)
	if err != nil {
		log.Printf("broadcast marshal: %v", err)
		return
	}

	s.mu.Lock()
	clients := make([]*websocket.Conn, 0, len(s.clients))
	for client := range s.clients {
		clients = append(clients, client)
	}
	s.mu.Unlock()

	for _, client := range clients {
		s.writeMu.Lock()
		err := client.WriteMessage(websocket.TextMessage, payload)
		s.writeMu.Unlock()
		if err != nil {
			client.Close()
			s.mu.Lock()
			delete(s.clients, client)
			s.mu.Unlock()
		}
	}
}

func (s *Store) handleLog(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}

	var event Event
	if err := json.NewDecoder(r.Body).Decode(&event); err != nil {
		http.Error(w, "invalid json", http.StatusBadRequest)
		return
	}

	record := s.appendEvent(event)
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusCreated)
	_ = json.NewEncoder(w).Encode(record)
}

func (s *Store) handleLogs(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}

	s.mu.Lock()
	defer s.mu.Unlock()
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(s.logs)
}

func (s *Store) handleLatest(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}

	s.mu.Lock()
	defer s.mu.Unlock()
	w.Header().Set("Content-Type", "application/json")
	if len(s.logs) == 0 {
		_ = json.NewEncoder(w).Encode(nil)
		return
	}
	_ = json.NewEncoder(w).Encode(s.logs[len(s.logs)-1])
}

func (s *Store) handleWS(w http.ResponseWriter, r *http.Request) {
	conn, err := s.upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("ws upgrade: %v", err)
		return
	}

	s.mu.Lock()
	s.clients[conn] = struct{}{}
	currentCount := len(s.logs)
	s.mu.Unlock()

	hello := Event{Type: "hello", Message: "connected"}
	if payload, err := json.Marshal(hello); err == nil {
		s.writeMu.Lock()
		_ = conn.WriteMessage(websocket.TextMessage, payload)
		s.writeMu.Unlock()
	}

	log.Printf("websocket connected, %d stored events", currentCount)

	go func() {
		defer func() {
			conn.Close()
			s.mu.Lock()
			delete(s.clients, conn)
			s.mu.Unlock()
		}()

		for {
			messageType, message, err := conn.ReadMessage()
			if err != nil {
				return
			}

			if messageType != websocket.TextMessage {
				continue
			}

			var event Event
			if err := json.Unmarshal(message, &event); err == nil {
				event.Type = firstNonEmpty(event.Type, "control")
				_ = s.appendEvent(event)
				continue
			}

			_ = s.appendEvent(Event{Type: "ws_text", Message: string(message)})
		}
	}()
}

func firstNonEmpty(values ...string) string {
	for _, value := range values {
		if value != "" {
			return value
		}
	}
	return ""
}

func main() {
	backendDir, err := os.Getwd()
	if err != nil {
		log.Fatal(err)
	}
	logPath := filepath.Join(backendDir, "logs.json")
	store := newStore(logPath)
	store.load()

	mux := http.NewServeMux()
	mux.HandleFunc("/log", store.handleLog)
	mux.HandleFunc("/logs", store.handleLogs)
	mux.HandleFunc("/latest", store.handleLatest)
	mux.HandleFunc("/ws", store.handleWS)

	server := &http.Server{
		Addr:    ":5000",
		Handler: mux,
	}

	log.Printf("Go backend running on %s", server.Addr)
	if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		log.Fatal(err)
	}
}
