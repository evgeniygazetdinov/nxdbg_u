.PHONY: all

all: mainwindow_gen.py

test: all

mainwindow_gen.py: mainwindow.ui
	pyuic5 -x mainwindow.ui -o mainwindow_gen.py

clean:
	rm mainwindow_gen.py *.pyc
