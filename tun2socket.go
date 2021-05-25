package tun2socket

import (
	"errors"
)

type Stack interface {
	Link() Link
	TCP() TCP
	UDP() UDP
	Close() error
}

type stack struct {
	link Link
	tcp  TCP
	udp  UDP
}

func (s *stack) Link() Link {
	return s.link
}

func (s *stack) TCP() TCP {
	return s.tcp
}

func (s *stack) UDP() UDP {
	return s.udp
}

func (s *stack) Close() error {
	_ = s.link.Close()
	_ = s.tcp.Close()
	_ = s.udp.Close()

	return nil
}

func NewStack(mtu int) (Stack, error) {
	link, err := NewLink(mtu)
	if err != nil {
		return nil, errors.New("unable to attach link")
	}

	tcp, err := ListenTCP()
	if err != nil {
		_ = link.Close()

		return nil, errors.New("unable to listen tcp")
	}

	udp, err := ListenUDP()
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
