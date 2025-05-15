// Copyright 2019 The Vearch Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.

package slog

import (
	"fmt"
	"os"
)

func NewSLog(dir, module, level string, toConsole bool) *sLog {
	var l loggingT
	l.dir = dir
	l.module = module
	l.alsoToStderr = toConsole
	var ok bool
	if l.outputLevel, ok = severityByName(level); !ok {
		panic("Unknown output log level")
	}
	ToInit(&l)
	return &sLog{l: &l}
}

type sLog struct {
	l *loggingT
}

func (l *sLog) IsDebugEnabled() bool {
	return l.l.outputLevel == debugLog
}

func (l *sLog) IsTraceEnabled() bool {
	return int(l.l.outputLevel) <= TRACE
}

func (l *sLog) IsInfoEnabled() bool {
	return int(l.l.outputLevel) <= INFO
}

func (l *sLog) IsWarnEnabled() bool {
	return int(l.l.outputLevel) <= WARN
}

func (l *sLog) Errorf(format string, v ...interface{}) {
	if errorLog >= l.l.outputLevel {
		l.l.printDepth(errorLog, 1, format, v...)
	}
}

func (l *sLog) Infof(format string, v ...interface{}) {
	if infoLog >= l.l.outputLevel {
		l.l.printDepth(infoLog, 1, format, v...)
	}
}

func (l *sLog) Debugf(format string, v ...interface{}) {
	if debugLog >= l.l.outputLevel {
		l.l.printDepth(debugLog, 1, format, v...)
	}
}

func (l *sLog) Tracef(format string, v ...interface{}) {
	if traceLog >= l.l.outputLevel {
		l.l.printDepth(traceLog, 1, format, v...)
	}
}

func (l *sLog) Warnf(format string, v ...interface{}) {
	if warningLog >= l.l.outputLevel {
		l.l.printDepth(warningLog, 1, format, v...)
	}
}

func (l *sLog) Panicf(format string, v ...interface{}) {
	if panicLog >= l.l.outputLevel {
		l.l.printDepth(panicLog, 1, format, v...)
	}
	l.l.lockAndFlushAll()
	panic(fmt.Sprintf(format, v...))
}

func (l *sLog) Fatalf(format string, v ...interface{}) {
	if fatalLog >= l.l.outputLevel {
		l.l.printDepth(fatalLog, 1, format, v...)
	}
	os.Exit(-1)
}

func (l *sLog) Flush() {
	l.l.lockAndFlushAll()
}

func (l *sLog) Error(v ...interface{}) {
	format := ""
	if errorLog >= l.l.outputLevel {
		if len(v) <= 1 {
			format = fmt.Sprint(v...)
		} else {
			format = v[0].(string)
			format = fmt.Sprintf(format, v[1:]...)
		}
		l.l.printDepth(errorLog, 1, format)
	}
}

func (l *sLog) Info(v ...interface{}) {
	format := ""
	if infoLog >= l.l.outputLevel {
		if len(v) <= 1 {
			format = fmt.Sprint(v...)
		} else {
			format = v[0].(string)
			format = fmt.Sprintf(format, v[1:]...)
		}
		l.l.printDepth(infoLog, 1, format)
	}
}

func (l *sLog) Trace(v ...interface{}) {
	format := ""
	if traceLog >= l.l.outputLevel {
		if len(v) <= 1 {
			format = fmt.Sprint(v...)
		} else {
			format = v[0].(string)
			format = fmt.Sprintf(format, v[1:]...)
		}
		l.l.printDepth(traceLog, 1, format)
	}
}

func (l *sLog) Debug(v ...interface{}) {
	format := ""
	if debugLog >= l.l.outputLevel {
		if len(v) <= 1 {
			format = fmt.Sprint(v...)
		} else {
			format = v[0].(string)
			format = fmt.Sprintf(format, v[1:]...)
		}
		l.l.printDepth(debugLog, 1, format)
	}
}

func (l *sLog) Warn(v ...interface{}) {
	format := ""
	if warningLog >= l.l.outputLevel {
		if len(v) <= 1 {
			format = fmt.Sprint(v...)
		} else {
			format = v[0].(string)
			format = fmt.Sprintf(format, v[1:]...)
		}
		l.l.printDepth(warningLog, 1, format)
	}
}

func (l *sLog) Panic(v ...interface{}) {
	format := ""
	if panicLog >= l.l.outputLevel {
		if len(v) <= 1 {
			format = fmt.Sprint(v...)
		} else {
			format = v[0].(string)
			format = fmt.Sprintf(format, v[1:]...)
		}
		l.l.printDepth(panicLog, 1, format)
	}
	l.l.lockAndFlushAll()
	panic(fmt.Sprintf(format))
}

func (l *sLog) Fatal(v ...interface{}) {
	format := ""
	if errorLog >= l.l.outputLevel {
		if len(v) <= 1 {
			format = fmt.Sprint(v...)
		} else {
			format = v[0].(string)
			format = fmt.Sprintf(format, v[1:]...)
		}
		l.l.printDepth(errorLog, 1, format)
	}
	os.Exit(-1)
}
