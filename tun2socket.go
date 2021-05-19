package tun2socket

import (
	"errors"

	"github.com/kr328/tun2socket/bridge"
)

type Stack interface {
	Link() bridge.Link
	TCP() bridge.TCP
	UDP() bridge.UDP
}

type stack struct {
	link bridge.Link
	tcp  bridge.TCP
	udp  bridge.UDP
}

func (s *stack) Link() bridge.Link {
	return s.link
}

func (s *stack) TCP() bridge.TCP {
	return s.tcp
}

func (s *stack) UDP() bridge.UDP {
	return s.udp
}

func (s *stack) Close() error {
	_ = s.link.Close()
	_ = s.tcp.Close()
	_ = s.udp.Close()

	return nil
}

func NewStack(mtu int) (Stack, error) {
	link, err := bridge.NewLink(mtu)
	if err != nil {
		return nil, errors.New("unable to attach link")
	}

	tcp, err := bridge.ListenTCP()
	if err != nil {
		_ = link.Close()

		return nil, errors.New("unable to listen tcp")
	}

	udp, err := bridge.ListenUDP()
	if err != nil {
		_ = link.Close()
		_ = tcp.Close()

		return nil, errors.New("unable to listen udp")
	}

	return &stack{
		link: link,
		tcp:  tcp,
		udp:  udp,
	}, nil
}
