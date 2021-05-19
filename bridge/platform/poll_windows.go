// +build windows

package platform

func PollRead(reader io.Reader, buf []byte) (int, error) {
	return 0, ErrNotImplement
}
