package main

import (
	"crypto/md5"
	"encoding/hex"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"strings"
)

const generatedFile = `
// Code generated .* DO NOT EDIT.

// +build %%%GOOS%%%,%%%GOARCH%%%

package %%%PACKAGE%%%

/*
#cgo CFLAGS: -I%%%NATIVE_PATH%%%
#cgo LDFLAGS: -L%%%BUILD_PATH%%% -lnative -llwip
*/
import "C"

const BuildLwipMD5 = "%%%LWIP_MD5%%%"
const BuildNativeMD5 = "%%%NATIVE_MD5%%%"
`

func main() {
	if len(os.Args) < 5 {
		println("invalid arguments")

		os.Exit(-1)
	}

	packageName := os.Args[1]
	nativePath, _ := filepath.Abs(os.Args[2])
	buildPath, _ := filepath.Abs(os.Args[3])
	GOOS := os.Args[4]
	GOARCH := os.Args[5]

	cmakeDir := path.Join(buildPath, GOOS, GOARCH)

	err := os.MkdirAll(cmakeDir, 0700)
	if err != nil {
		println("MkdirAll:", err.Error())

		os.Exit(1)
	}

	cmakeCmd := []string{"cmake", "-DCMAKE_BUILD_TYPE=Release"}

	for _, e := range os.Environ() {
		if strings.HasPrefix(e, "CMAKE") {
			cmakeCmd = append(cmakeCmd, "-D"+e)
		}
	}

	cmakeCmd = append(cmakeCmd, nativePath)

	runCommand(cmakeCmd, cmakeDir, nil)

	buildCmd := []string{"cmake", "--build", "."}

	runCommand(buildCmd, cmakeDir, nil)

	replacer := strings.NewReplacer(
		"%%%PACKAGE%%%", packageName,
		"%%%GOOS%%%", GOOS,
		"%%%GOARCH%%%", GOARCH,
		"%%%NATIVE_PATH%%%", nativePath,
		"%%%BUILD_PATH%%%", cmakeDir,
		"%%%LWIP_MD5%%%", fileMd5(path.Join(cmakeDir, "liblwip.a")),
		"%%%NATIVE_MD5%%%", fileMd5(path.Join(cmakeDir, "libnative.a")),
	)

	err = ioutil.WriteFile(
		"build_"+GOOS+"_"+GOARCH+".go",
		[]byte(strings.TrimSpace(replacer.Replace(generatedFile))),
		0644,
	)
	if err != nil {
		println("Write generated file:", err.Error())

		os.Exit(1)
	}
}

func runCommand(command []string, pwd string, envs []string) {
	cmd := exec.Command(command[0], command[1:]...)

	cmd.Env = envs
	cmd.Dir = pwd
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	err := cmd.Run()
	if err != nil {
		fmt.Printf("%s: %s", strings.Join(command, " "), err.Error())

		os.Exit(1)
	}
}

func fileMd5(path string) string {
	file, err := os.Open(path)
	if err != nil {
		println("Open " + path + ": " + err.Error())

		os.Exit(1)
	}

	m := md5.New()

	_, err = io.Copy(m, file)
	if err != nil {
		println("Read " + path + ": " + err.Error())

		os.Exit(1)
	}

	return hex.EncodeToString(m.Sum(nil))
}
