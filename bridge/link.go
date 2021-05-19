package bridge

//#include "device.h"
import "C"

import (
	"io"
	"sync"
	"unsafe"
)

var (
	globalLinkLock = sync.Mutex{}
)

type Link struct {
	device io.ReadWriteCloser
	mtu    int
}

func (l *Link) Start() error {
	go l.process()

	return nil
}

func NewLink(device io.ReadWriteCloser, mtu int) *Link {
	return &Link{
		device: device,
		mtu:    mtu,
	}
}

func (l *Link) process() {
	globalLinkLock.Lock()
	defer globalLinkLock.Unlock()

	context := C.new_device_context(C.int(l.mtu))

	C.device_attach(context)

	go func() {
		// lwip -> tun device

		buffer := make([]byte, l.mtu)

		defer C.device_free(context)

		for {
			n := C.device_read_rx_packet(context, unsafe.Pointer(&buffer[0]), C.int(len(buffer)))

			if n < 0 {
				return
			} else if n == 0 {
				continue
			}

			_, _ = l.device.Write(buffer[:n])
		}
	}()

	// tun device -> lwip

	buffer := make([]byte, l.mtu)

	defer C.device_close(context)

	for {
		n, err := l.device.Read(buffer)
		if err != nil {
			return
		}

		C.device_write_tx_packet(context, unsafe.Pointer(&buffer[0]), C.int(n))
	}
}
