.PHONY: clean native install

native:
	make -C native

install: native
	python setup.py install

clean:
	make -C native clean
