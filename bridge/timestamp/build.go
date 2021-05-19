package main

import (
	"os"
	"strings"
	"time"
)

func main() {
	if len(os.Args) < 2 {
		panic("invalid arguments")
	}

	lastModified := time.Time{}

	for i := 1; i < len(os.Args)-1; i++ {
		stat, err := os.Stat(os.Args[i])
		if err != nil {
			panic(err.Error())
		}

		if stat.ModTime().After(lastModified) {
			lastModified = stat.ModTime()
		}
	}

	sb := strings.Builder{}

	sb.WriteString("// Code generated .* DO NOT EDIT.\n\n")
	sb.WriteString("package bridge\n\n")
	sb.WriteString("const NativeBuildTime = `")
	sb.WriteString(lastModified.String())
	sb.WriteString("`\n")

	err := os.WriteFile(os.Args[len(os.Args)-1], []byte(sb.String()), 0600)
	if err != nil {
		panic(err.Error())
	}
}
