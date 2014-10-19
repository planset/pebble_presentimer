package main

import (
	"fmt"
	"log"
	"net/http"
	"os"
	"os/exec"
)

func control(cmd string) {
	_, err := exec.Command("/usr/bin/osascript", "bin/"+cmd+".scpt").Output()
	if err != nil {
		log.Fatal(err)
	}
	fmt.Printf("%s\n", cmd)
}

func nextHandler(w http.ResponseWriter, r *http.Request) {
	const cmd = "next"
	control(cmd)
	fmt.Fprintf(w, cmd)
}

func backHandler(w http.ResponseWriter, r *http.Request) {
	const cmd = "back"
	control(cmd)
	fmt.Fprintf(w, cmd)
}

func main() {
	port := "18888"
	if len(os.Args) >= 2 {
		port = os.Args[1]
	}

	fmt.Printf("Starting server ... 0.0.0.0:" + port + "\n")

	http.HandleFunc("/next", nextHandler)
	http.HandleFunc("/back", backHandler)
	err := http.ListenAndServe("0.0.0.0:"+port, nil)
	if err != nil {
		log.Fatal(err)
	}
}
