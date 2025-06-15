package config

import (
	"fmt"
	"master/utils/log"
	"net"
	"os"
	"regexp"
	"strings"
	"sync"

	"github.com/BurntSushi/toml"
)

// Model start up model, include all, master, ps, router
type Model int

var single *Config

// Conf return the single instance of config
func Conf() *Config {
	return single
}

var (
	versionOnce        sync.Once
	buildVersion       = "0.0"
	buildTime          = "0"
	commitID           = "xxxxx"
	LogInfoPrintSwitch = false
	Trace              = false
)
var NodesPrefix string = "/nodes"
var PsNodesPrefix string = NodesPrefix + "/ps"
var RouterNodesPrefix string = NodesPrefix + "/router"
var SpacesMataPrefix string = "/spaces/mata"
var CreatingSpacesMataPrefix string = "/creating_spaces/mata"
var DeletedSpacesMataPrefix string = "/deleted_spaces/mata"
var DdlLockKey string = "/ddl/lock"
var DdlDisabledKey string = "/cluster/ddl_disabled"
var ErrSpacesPrefix string = "/error/spaces"
var ErrPsNodesPrefix string = "/error/nodes/ps"
var ErrLog string = "/error/log"

var EtcdValMaxLen int = 100    // 100KB
var ErrLogValMaxLen int = 1024 // 1MB

const (
	GenPsIdKey    = "gen_ps_id"
	GenSpaceIdKey = "gen_space_id"
	IdBaseVal     = 0
)

// SetConfigVersion set the version, time and commit id of build
func SetConfigVersion(bv, bt, ci string) {
	versionOnce.Do(func() {
		buildVersion = bv
		buildTime = bt
		commitID = ci
	})
}

func GetBuildVersion() string {
	return buildVersion
}
func GetBuildTime() string {
	return buildTime
}
func GetCommitID() string {
	return commitID
}

const (
	Master Model = iota
	PS
	Router
)

const (
	LocalSingleAddr = "127.0.0.1"
	LocalCastAddr   = "0.0.0.0"
)

type Base struct {
	Log         string   `toml:"log,omitempty" json:"log"`
	Level       string   `toml:"level,omitempty" json:"level"`
	LogFileNum  int      `toml:"log_file_num,omitempty" json:"log_file_num"`
	LogFileSize int      `toml:"log_file_size,omitempty" json:"log_file_size"`
	Data        []string `toml:"data,omitempty" json:"data"`
}

type GlobalCfg struct {
	Base
	Name            string `toml:"name,omitempty" json:"name"`
	ResourceName    string `toml:"resource_name,omitempty" json:"resource_name"`
	Path            string `toml:"path,omitempty" json:"path"`
	SupportEtcdAuth bool   `toml:"support_etcd_auth,omitempty" json:"support_etcd_auth"`
}

type EtcdCfg struct {
	AddressList    []string `toml:"address,omitempty" json:"address"`
	EtcdClientPort uint16   `toml:"etcd_client_port,omitempty" json:"etcd_client_port"`
	Username       string   `toml:"user_name,omitempty" json:"user_name"`
	Password       string   `toml:"password,omitempty" json:"password"`
}

type TracerCfg struct {
	Host        string  `toml:"host,omitempty" json:"host"`
	SampleType  string  `toml:"sample_type,omitempty" json:"sample_type"`
	SampleParam float64 `toml:"sample_param,omitempty" json:"sample_param"`
}

type Config struct {
	Global     *GlobalCfg `toml:"global,omitempty" json:"global"`
	EtcdConfig *EtcdCfg   `toml:"etcd,omitempty" json:"etcd"`
	TracerCfg  *TracerCfg `toml:"tracer,omitempty" json:"tracer"`
	Masters    Masters    `toml:"masters,omitempty" json:"masters"`
	Router     *RouterCfg `toml:"router,omitempty" json:"router"`
	PS         *PSCfg     `toml:"ps,omitempty" json:"ps"`
	mu         *sync.RWMutex
}

// get etcd address config
func (con *Config) GetEtcdAddress() []string {
	// manage etcd by vearch
	ms := con.GetMasters()
	addrs := make([]string, len(ms))
	for i, m := range ms {
		addrs[i] = fmt.Sprintf("%s:%d", m.Address, ms[i].EtcdClientPort)
		log.Info("vearch etcd address is %s", addrs[i])
	}
	return addrs
}

func (c *Config) GetLogDir() string {
	return c.Global.Log
}

// make sure it not use in loop
func (c *Config) GetLevel() string {
	return c.Global.Level
}

func (c *Config) GetDataDir() string {
	return c.Global.Data[0]
}

func (c *Config) GetDataDirBySlot(model Model, pid uint32) string {
	s := c.Global.Data
	index := int(pid) % len(s)
	return s[index]
}

func (c *Config) GetDatas() []string {
	return c.Global.Data
}

func (c *Config) GetLogFileNum() int {
	return c.Global.LogFileNum
}

func (c *Config) GetLogFileSize() int {
	return c.Global.LogFileSize
}

func (c *Config) GetMasters() Masters {
	c.mu.RLock()
	defer c.mu.RUnlock()
	ms := make([]*MasterCfg, len(c.Masters))
	copy(ms, c.Masters)
	return ms
}

func (c *Config) SetMasters(newMasters Masters) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.Masters = newMasters
}

func (config *Config) CurrentByMasterNameDomainIp(masterName string) error {
	//find local all ip
	addrMap := config.addrMap()
	var found bool
	for _, m := range config.Masters {
		var domainIP *net.IPAddr
		// check if m.Address is a ip
		match, _ := regexp.MatchString(`^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/[0-9]+$`, m.Address)
		if !match {
			// if not match, search DNS IP by domainName
			tempIP, err := net.ResolveIPAddr("ip", m.Address)
			if err != nil {
				log.Errorf("address [%s] has err: %v", m.Address, err)
				return err
			}
			domainIP = tempIP
			log.Info("master's name:[%s] master's domain:[%s] and local master's ip:[%s]",
				m.Name, m.Address, domainIP)
		}
		if m.Name == masterName {
			m.Self = true
			found = true
		} else if addrMap[m.Address] || (domainIP != nil && addrMap[domainIP.String()]) {
			log.Info("found local master successfully :master's name:[%s] master's ip:[%s] and local master's name:[%s]", m.Name, m.Address, masterName)
			m.Self = true
			found = true
		} else {
			log.Info("find local master failed:master's name:[%s] master's ip:[%s] and local master's name:[%s]", m.Name, m.Address, masterName)
		}
		if found {
			return nil
		}
	}
	return fmt.Errorf("None of the masters has the same ip address as current local master server's ip")
}

func (config *Config) addrMap() map[string]bool {
	addrMap := map[string]bool{LocalSingleAddr: true, LocalCastAddr: true}
	ifaces, err := net.Interfaces()
	if err != nil {
		panic(err)
	}
	for _, i := range ifaces {
		addrs, _ := i.Addrs()
		for _, addr := range addrs {
			match, _ := regexp.MatchString(`^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/[0-9]+$`, addr.String())
			if !match {
				continue
			}
			slit := strings.Split(addr.String(), "/")
			addrMap[slit[0]] = true
		}
	}
	return addrMap
}

func (config *Config) Validate(model Model) error {
	switch model {
	case Master:
		masterNum := 0
		for _, m := range config.Masters {
			if m.Self {
				masterNum++
			}
		}
		if masterNum > 1 {
			return fmt.Errorf("two masters on one machine")
		}
	}

	return config.validatePath()
}

func (config *Config) validatePath() error {
	if err := os.MkdirAll(config.GetLogDir(), os.ModePerm); err != nil {
		return err
	}
	if err := os.MkdirAll(config.GetDataDir(), os.ModePerm); err != nil {
		return err
	}

	return nil
}

func InitConfig(path string) {
	single = &Config{
		mu:     new(sync.RWMutex),
		Global: &GlobalCfg{},
		PS:     &PSCfg{},
	}
	LoadConfig(single, path)
}

func LoadConfig(conf *Config, path string) {
	if len(path) == 0 {
		log.Error("load config path is empty")
		os.Exit(-1)
	}
	if _, err := toml.DecodeFile(path, conf); err != nil {
		log.Error("decode:[%s] failed, err:[%s]", path, err.Error())
		os.Exit(-1)
	}
	conf.Global.Path = path
}

func DumpConfig(conf *Config) error {
	if len(conf.Global.Path) == 0 {
		msg := "dump config path is empty"
		log.Error(msg)
		return fmt.Errorf(msg)
	}
	fp, err := os.Create(conf.Global.Path)
	if err != nil {
		return err
	}
	defer fp.Close()
	if err := toml.NewEncoder(fp).Encode(conf); err != nil {
		log.Error("encode:[%s] failed, err:[%s]", conf.Global.Path, err.Error())
		return err
	}
	return nil
}
