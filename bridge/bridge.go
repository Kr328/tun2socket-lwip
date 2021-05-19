package bridge

//go:generate cmake -E make_directory build
//go:generate cmake -E chdir build cmake $CMAKE_ARGS ../native
//go:generate cmake -E chdir build cmake --build .
//go:generate go run timestamp/build.go build/libnative.a build/liblwip.a build.go

/*
#cgo CFLAGS: -I${SRCDIR}/native
#cgo LDFLAGS: -L${SRCDIR}/build -lnative -llwip

#include "init.h"
*/
import "C"

import "errors"

var ErrUnsupported = errors.New("unsupported")
var ErrNative = errors.New("native error")

func init() {
	C.init_lwip()
}
