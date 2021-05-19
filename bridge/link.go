package bridge

/*
#include "device.h"

#include <malloc.h>
*/
import "C"

import (
	"io"
	"sync"
	"unsafe"

	"github.com/kr328/tun2socket/bridge/platform"
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

		buffers := make([][]byte, int(C.DEVICE_BUFFER_ARRAY_SIZE))
		nativeBuffers := (*C.device_buffer_array_t)(C.malloc(C.sizeof_device_buffer_array_t))
		defer C.device_free(context)
		defer C.free(unsafe.Pointer(nativeBuffers))

		for i := 0; i < len(buffers); i++ {
			buffers[i] = make([]byte, l.mtu)
			nativeBuffers.buffers[i].data = unsafe.Pointer(&buffers[i][0])
			nativeBuffers.buffers[i].length = 0
		}

		for {
			for i := 0; i < len(buffers); i++ {
				nativeBuffers.buffers[i].length = C.int(len(buffers[i]))
			}

			n := C.device_read_rx_packets(context, nativeBuffers)

			if n < 0 {
				return
			}

			for i := 0; i < int(n); i++ {
				length := int(nativeBuffers.buffers[i].length)

				if length < 0 {
					continue
				}

				_, err := l.device.Write(buffers[i][:length])
				if err != nil {
					println(err.Error())
				}
			}
		}
	}()

	// tun device -> lwip

	buffers := make([][]byte, int(C.DEVICE_BUFFER_ARRAY_SIZE))
	nativeBuffers := (*C.device_buffer_array_t)(C.malloc(C.sizeof_device_buffer_array_t))
	defer C.device_close(context)
	defer C.free(unsafe.Pointer(nativeBuffers))

	for i := 0; i < len(buffers); i++ {
		buffers[i] = make([]byte, l.mtu)
		nativeBuffers.buffers[i].data = unsafe.Pointer(&buffers[i][0])
		nativeBuffers.buffers[i].length = 0
	}

	for {
		n, err := l.device.Read(buffers[0][:])
		if err != nil {
			return
		}

		nativeBuffers.buffers[0].length = C.int(n)

		var c int

		for c = 1; c < len(buffers); c++ {
			n, err := platform.PollRead(l.device, buffers[c])
			if err != nil {
				break
			}

			nativeBuffers.buffers[c].length = C.int(n)
		}

		C.device_write_tx_packets(context, nativeBuffers, C.int(c))
	}
}
