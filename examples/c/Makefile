all: dealer router

dealer:
	gcc dealer.c -lzmq -lczmq -o dealer

router:
	gcc router.c -lzmq -lczmq -o router

clean:
	rm dealer router
