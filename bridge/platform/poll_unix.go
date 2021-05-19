// +build darwin dragonfly freebsd linux netbsd openbsd solaris

package platform

import (
	"io"
	"syscall"
)

func PollRead(reader io.Reader, buf []byte) (int, error) {
	s, ok := reader.(syscall.Conn)
	if !ok {
		return 0, ErrNotImplement
	}

	conn, err := s.SyscallConn()
	if err != nil {
		return 0, err
	}

	var n int
	var inner error

	err = conn.Control(func(fd uintptr) {
		n, inner = syscall.Read(int(fd), buf)
	})
	if err != nil {
		return 0, err
	}

	return n, inner
}
