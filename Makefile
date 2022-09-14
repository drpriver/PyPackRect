ifeq ($(OS),Windows_NT)
UNAME:=Windows
else
UNAME:=$(shell uname)
endif

.PHONY: wheels
ifeq ($(UNAME),Darwin)
civenv:
	python3 -m venv civenv
	. civenv/bin/activate && python -m pip install cibuildwheel && python -m pip install twine

# macos you need to build multiple times
wheels: civenv
	rm -rf dist build
	rm -f wheelhouse/*.whl
	. civenv/bin/activate && CIBW_SKIP='{pp*,cp36*,cp37*,cp311*}' cibuildwheel --platform macos --archs x86_64 .
	. civenv/bin/activate && CIBW_SKIP='{pp*,cp36*,cp37*,cp311*}' cibuildwheel --platform macos --archs arm64 .
	. civenv/bin/activate && CIBW_SKIP='{pp*,cp36*,cp37*,cp311*}' cibuildwheel --platform macos --archs universal2 .
endif

ifeq ($(UNAME),Linux)
civenv:
	python3 -m venv civenv
	. civenv/bin/activate && python -m pip install cibuildwheel && python -m pip install twine

wheels: civenv
	rm -rf dist build
	rm -f wheelhouse/*.whl
	. civenv/bin/activate && CIBW_SKIP='{pp*,*musl*,cp311*}' cibuildwheel --platform linux --archs x86_64 .
endif

ifeq ($(UNAME),Windows)
civenv:
	py -m venv civenv
	civenv\Scripts\activate && py -m pip install cibuildwheel && py -m pip install twine

wheels: civenv
	del /q dist build
	civenv\Scripts\activate && cmd /V /C "SET CIBW_SKIP={pp*,cp36*,cp37*,cp311*} && cibuildwheel --platform windows --archs AMD64 ."
endif


.PHONY: pypi-upload
pypi-upload: archive-wheels
	. civenv/bin/activate && python3 -m twine upload wheelhouse/* --verbose

.PHONY: archive-wheels
archive-wheels: | ArchivedWheels
	cp wheelhouse/*.whl ArchivedWheels
ArchivedWheels: ; mkdir $@

include $(wildcard gather.mak)
