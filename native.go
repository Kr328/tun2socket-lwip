package tun2socket

//go:generate go run make/make.go $GOPACKAGE . $GOOS $GOARCH

/*
#cgo CFLAGS: -Inative

#include "init.h"
*/
import "C"

import "errors"

var ErrUnsupported = errors.New("unsupported")
var ErrNative = errors.New("native error")

func init() {
	C.init_lwip()
}
