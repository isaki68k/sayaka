all:
	$(MAKE) -C mbedtls lib
	$(MAKE) -C vala vala-make2
	$(MAKE) -C vala

clean:
	$(MAKE) -C mbedtls/lib clean
	$(MAKE) -C vala clean
