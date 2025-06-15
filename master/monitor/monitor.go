package monitor

import "fmt"

func AddErrorInfo(errType string, leval int, info string) {
	fmt.Print("AddErrorInfo called with type: ", errType, " and info: ", info)
}
