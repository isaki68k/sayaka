all:
	$(MAKE) -C mbedtls programs
	$(MAKE) -C vala vala-make2
	$(MAKE) -C vala
