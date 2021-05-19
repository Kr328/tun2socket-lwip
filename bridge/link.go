package bridge

//#include "link.h"
import "C"

import (
	"runtime"
	"unsafe"
)

type Link interface {
	Read(buf []byte) (int, error)
	Write(buf []byte) (int, error)
	Close() error
}

type link struct {
	context *C.link_t
}

func (l *link) Read(buf []byte) (int, error) {
	n := C.link_read(l.context, unsafe.Pointer(&buf[0]), C.int(len(buf)))
	if n < 0 {
		return 0, ErrNative
	}

	return int(n), nil
}

func (l *link) Write(buf []byte) (int, error) {
	n := C.link_write(l.context, unsafe.Pointer(&buf[0]), C.int(len(buf)))
	if n < 0 {
		return 0, ErrNative
	}

	return int(n), nil
}

func (l *link) Close() error {
	C.link_close(l.context)

	return nil
}

func NewLink(mtu int) (Link, error) {
	context := C.link_attach(C.int(mtu))
	if context == nil {
		return nil, ErrNative
	}

	l := &link{context: context}

	runtime.SetFinalizer(l, linkDestroy)

	return l, nil
}

func linkDestroy(l *link) {
	C.link_free(l.context)
}
