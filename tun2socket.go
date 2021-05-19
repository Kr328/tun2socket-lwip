package tun2socket

import (
	"io"
	"net"

	"github.com/kr328/tun2socket/bridge"
)

type LwipStack struct {
	*bridge.Link
	net.Listener
	bridge.PacketConn
}

func (s *LwipStack) Close() error {
	s.Listener.Close()
	s.PacketConn.Close()

	return nil
}

func NewLwipStack(device io.ReadWriteCloser, mtu int) (*LwipStack, error) {
	link := bridge.NewLink(device, mtu)

	listener, err := bridge.Listen()
	if err != nil {
		return nil, err
	}

	packet, err := bridge.ListenPacket()
	if err != nil {
		listener.Close()

		return nil, err
	}

	return &LwipStack{
		Link:       link,
		Listener:   listener,
		PacketConn: packet,
	}, nil
}
