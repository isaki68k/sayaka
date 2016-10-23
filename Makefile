all:
	$(MAKE) -C mbedtls lib
	$(MAKE) -C vala vala-make2
	$(MAKE) -C vala
