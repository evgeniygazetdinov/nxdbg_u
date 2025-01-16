# Copyright 2017 plutoo
from PyQt5 import QtGui, QtWidgets
from PyQt5.QtCore import QThread, pyqtSignal


import sys
import time
import threading
import mainwindow_gen
import ArmDisassembler
import AddressFormatter
from PyQt5 import uic
from RemoteConnection import *
from RemoteConnectionUsb import *
from RemoteConnectionTest import *

from Utils import *
from Lazy import *
from Playground import *
from ExpressionEval import *
from BreakpointManager import *

from AdapterMemoryLayout import *
from AdapterThreadList import *
from AdapterStateLabel import *
from AdapterRegisters import *
from AdapterStackTrace import *
from AdapterView import *


class EventReceiveThread(QThread):
    def __init__(self, usb, dbg_handle):
        QThread.__init__(self)
        self.usb = usb
        self.dbg_handle = dbg_handle
        self.event = threading.Event()

    # def __del__(self):
    #     self.wait()

    def run(self):
        while 1:
            try:
                event = self.usb.cmdGetDbgEvent(self.dbg_handle)

                self.event.clear()
                self.emit(pyqtSignal('onDbgEvent(PyQt_PyObject)'), event)
                self.event.wait()

            except SwitchError:
                pass
            time.sleep(0.1)


class MainDebugger(QtWidgets.QMainWindow):
    def __init__(self, usb, dbg_handle, parent=None):
        super(MainDebugger, self).__init__(parent)
        uic.loadUi('mainwindow.ui', self)
        # self.setupUi(self)


        self.active_event = None

        self.usb = usb
        self.dbg_handle = dbg_handle
        self.usb_thread = EventReceiveThread(usb, dbg_handle)

        AddressFormatter.AddressFormatter(self.usb, self.dbg_handle)

        self.bp_manager = BreakpointManager(self.usb, self.dbg_handle, self.treeBreakpoints)
        expr_eval = ExpressionEvaluator(self.usb, self.dbg_handle, self)
        self.expr_eval = expr_eval

        self.adapters = []
        self.adapters.append(Lazy(usb, dbg_handle))
        self.adapters.append(AdapterMemoryLayout(usb, dbg_handle, self.treeMemory))
        self.adapters.append(AdapterThreadList(self.treeThreads))
        self.adapters.append(AdapterStateLabel(self.labelState))
        self.adapters.append(AdapterRegisters(usb, dbg_handle, self))
        self.adapters.append(AdapterStackTrace(usb, dbg_handle, self.treeStackTrace))
        self.adapters.append(AdapterView(usb, dbg_handle, expr_eval, self.lineCmdView0, self.textOutputView0))
        self.adapters.append(AdapterView(usb, dbg_handle, expr_eval, self.lineCmdView1, self.textOutputView1))
        self.adapters.append(AdapterView(usb, dbg_handle, expr_eval, self.lineCmdView2, self.textOutputView2))
        self.adapters.append(Playground(usb, dbg_handle, self))

        self.connect(self.usb_thread, pyqtSignal('onDbgEvent(PyQt_PyObject)'), self.onDbgEvent)
        self.usb_thread.start()

        self.lineCmd.returnPressed.connect(self.onUserCmd)

    def onUserCmd(self):
        self.expr_eval.execute(self.textOutput, self.lineCmd)
        self.lineCmd.setText('')

    def closeEvent(self, event):
        self.bp_manager.cleanup()
        try: self.usb.cmdContinueDbgEvent(self.dbg_handle, 4|2|1, 0)
        except: pass
        try: self.usb.cmdDetachProcess(self.dbg_handle)
        except: pass
        event.accept()

    def outGrey(self, text):
        self.textOutput.setTextColor(QtGui.QColor(0x77, 0x77, 0x77))
        self.textOutput.append(text)

    def outRed(self, text):
        self.textOutput.setTextColor(QtGui.QColor(0xFF, 0x33, 0x33))
        self.textOutput.append(text)

    def outBlack(self, text):
        self.textOutput.setTextColor(QtGui.QColor(0, 0, 0))
        self.textOutput.append(text)

    def onDbgEvent(self, event):
        assert self.active_event is None
        self.active_event = event
        self.dispatchDbgEvent(event)

    def continueDbgEvent(self):
        if self.active_event and requiresContinue(self.active_event):
            self.bp_manager.continueDbgEvent()

            try:
                self.usb.cmdContinueDbgEvent(self.dbg_handle, 4|2|1, 0)
            except SwitchError:
                pass

        self.active_event = None
        self.dispatchDbgEvent(None)
        self.usb_thread.event.set()

    def requestContinue(self):
        self.continueRequested = True

    def dispatchDbgEvent(self, event):
        if event:
            self.outGrey(repr(event))

        self.bp_manager.onDbgEvent(event)
        self.continueRequested = False

        for a in self.adapters:
            a.onDbgEvent(event)

        if self.continueRequested:
            self.continueDbgEvent()
            return

        if isinstance(event, ProcessAttachEvent) and not self.cbxProcessAttachEvent.isChecked():
            self.continueDbgEvent()
        if isinstance(event, ThreadAttachEvent) and not self.cbxThreadAttachEvent.isChecked():
            self.continueDbgEvent()
        if isinstance(event, Unknown2Event) and not self.cbxUnknownEvent.isChecked():
            self.continueDbgEvent()
        if isinstance(event, ExitEvent) and not self.cbxExitEvent.isChecked():
            self.continueDbgEvent()
        if isinstance(event, ExceptionEvent) and not self.cbxExceptionEvent.isChecked():
            self.continueDbgEvent()

def main(argv):
    usb = None
    dbg_handle = None
    if len(argv) > 1:
        if argv[1] == '--pid':
            pid = int(argv[2])
            usb = RemoteConnectionUsb()
            dbg_handle = usb.cmdAttachProcess(pid)

        elif argv[1] == '--titleid':
            titleid = int(argv[2], 0)
            usb = RemoteConnectionUsb()
            pid = usb.cmdGetTitlePid(titleid)
            dbg_handle = usb.cmdAttachProcess(pid)

        elif argv[1] == '--nextlaunch':
            usb = RemoteConnectionUsb()
            usb.cmdListenForAppLaunch()

            pid = None
            while pid is None:
                print('Waiting for launch..')
                time.sleep(1)
                pid = usb.cmdGetAppPid()

            dbg_handle = usb.cmdAttachProcess(pid)
            usb.cmdStartProcess(pid)

        elif argv[1] == '--test':
            usb = RemoteConnectionTest()
            dbg_handle = 0

        else:
            print('Usage: %s [--pid <pid>] || [--nextlaunch] || --titleid <tid>' % argv[0])
            return 0

    try:
        app = QtWidgets.QApplication(argv)
        #QtWidgets.QApplication.setStyle(QtGui.QStyleFactory.create('Cleanlooks'))
        form = MainDebugger(usb, dbg_handle)
        form.show()
        app.exec_()
    except Exception as e:
        usb.cmdDetachProcess(dbg_handle)
        raise e

    return 0

sys.exit(main(sys.argv))
