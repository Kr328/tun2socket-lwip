package main

import (
	"crypto/md5"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"io/ioutil"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"runtime"
	"strings"
	"time"
)

const generatedFile = `
// Code generated .* DO NOT EDIT.

// +build %%%GOOS%%%,%%%GOARCH%%%

package %%%PACKAGE%%%

/*
#cgo CFLAGS: -I"%%%NATIVE_PATH%%%" -fPIC -O3
#cgo LDFLAGS: -L"%%%BUILD_PATH%%%" -lnative
*/
import "C"

const BuildNativeMD5 = "%%%NATIVE_MD5%%%"
`

type Kind int

const (
	Header Kind = iota
	Source
)

type source struct {
	kind     Kind
	path     string
	modified time.Time
}

func main() {
	if len(os.Args) < 5 {
		println("Usage: <package-name> <project-root> <target-GOOS> <target-GOARCH>")

		os.Exit(-1)
	}

	packageName := os.Args[1]
	projectRoot, _ := filepath.Abs(os.Args[2])
	GOOS := os.Args[3]
	GOARCH := os.Args[4]

	buildDir := path.Join(projectRoot, "build", GOOS, GOARCH)
	target := path.Join(buildDir, "libnative.a")

	sources, err := collectSources(projectRoot)
	if err != nil {
		println("Collect sources: " + err.Error())

		os.Exit(1)
	}

	changed, err := filterChanged(sources, target)
	if err != nil {
		println("Filter changed: " + err.Error())

		os.Exit(1)
	}

	changed = filterSource(changed)

	if len(changed) == 0 {
		println("Everything update to date")

		return
	}

	cc := os.Getenv("CC")
	if cc == "" {
		cc, err = findCc()
		if err != nil {
			println("Search CC: " + err.Error())

			os.Exit(1)
		}
	}

	lwipInclude := path.Join(projectRoot, "lwip", "include")
	lwipArchInclude := path.Join(projectRoot, "lwip", "ports", "unix", "include")
	nativeInclude := path.Join(projectRoot, "native")

	cFlags := fmt.Sprintf(`-Ofast -fPIC -I"%s" -I"%s" -I"%s" %s`, lwipInclude, nativeInclude, lwipArchInclude, os.Getenv("CFLAGS"))
	objsDir := path.Join(buildDir, "objs")

	var objs []string

	for i, s := range changed {
		if s.kind == Header {
			continue
		}

		obj := path.Join(objsDir, s.path+".o")

		if err := os.MkdirAll(path.Dir(obj), 0700); err != nil {
			println("Create dir " + path.Dir(obj) + ": " + err.Error())

			os.Exit(1)
		}

		fmt.Printf("[%2d/%2d] %s\n", i+1, len(changed), s.path)

		runCommand(fmt.Sprintf(`"%s" %s -c -o "%s" "%s"`, cc, cFlags, obj, path.Join(projectRoot, s.path)))

		objs = append(objs, obj)
	}

	ar := os.Getenv("AR")
	if ar == "" {
		ar, err = exec.LookPath("ar")
		if err != nil {
			println("C archiver ar unavailable: " + err.Error())

			os.Exit(1)
		}
	}

	runCommand(fmt.Sprintf(`"%s" rcs "%s" %s`, ar, target, fmt.Sprintf(`"%s"`, strings.Join(objs, `" "`))))

	if GOOS == "ios" || GOOS == "darwin" {
		ranlib := os.Getenv("RANLIB")
		if ranlib == "" {
			ranlib, err = exec.LookPath("ranlib")
			if err != nil {
				println("ranlib unavailable: " + err.Error())

				os.Exit(1)
			}
		}

		runCommand(fmt.Sprintf(`"%s" -no_warning_for_no_symbols -c "%s"`, ranlib, target))
	}

	replacer := strings.NewReplacer(
		"%%%PACKAGE%%%", packageName,
		"%%%GOOS%%%", GOOS,
		"%%%GOARCH%%%", GOARCH,
		"%%%NATIVE_PATH%%%", path.Join(projectRoot, "native"),
		"%%%BUILD_PATH%%%", buildDir,
		"%%%NATIVE_MD5%%%", fileMd5(target),
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

func runCommand(command string) {
	var cmd *exec.Cmd

	if runtime.GOOS == "windows" {
		cmd = exec.Command("cmd.exe", "/c", command)
	} else {
		cmd = exec.Command("bash", "-c", command)
	}

	cmd.Stdin = nil
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	err := cmd.Run()
	if err != nil {
		fmt.Printf("%s: %s\n", command, err.Error())

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

func collectSources(root string) ([]*source, error) {
	var sources []*source

	return sources, filepath.Walk(root, func(path string, info fs.FileInfo, err error) error {
		if err != nil {
			return err
		}

		switch filepath.Ext(path) {
		case ".h":
			p, err := filepath.Rel(root, path)
			if err != nil {
				return err
			}

			s := &source{
				kind:     Header,
				path:     p,
				modified: info.ModTime(),
			}

			sources = append(sources, s)
		case ".c":
			p, err := filepath.Rel(root, path)
			if err != nil {
				return err
			}

			s := &source{
				kind:     Source,
				path:     p,
				modified: info.ModTime(),
			}

			sources = append(sources, s)
		}

		return nil
	})
}

func filterChanged(sources []*source, target string) ([]*source, error) {
	var changed []*source

	stat, err := os.Stat(target)
	if err != nil && !os.IsNotExist(err) {
		return nil, err
	}

	modified := time.Time{}
	if stat != nil {
		modified = stat.ModTime()
	}

	for _, s := range sources {
		if s.modified.Before(modified) {
			continue
		}

		if s.kind == Header {
			return sources, nil
		}

		changed = append(changed, s)
	}

	return changed, nil
}

func filterSource(sources []*source) []*source {
	var r []*source

	for _, s := range sources {
		if s.kind == Header {
			continue
		}

		r = append(r, s)
	}

	return r
}

func findCc() (string, error) {
	if p, err := exec.LookPath("clang"); err == nil {
		return p, nil
	}
	if p, err := exec.LookPath("gcc"); err == nil {
		return p, nil
	}

	return "", errors.New("C Compiler clang/gcc unavailable in PATH")
}
