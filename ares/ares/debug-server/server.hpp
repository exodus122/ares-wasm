#pragma once

#include <nall/tcptext/tcptext-server.hpp>
#include <ares/ares/debug-server/watchpoint.hpp>

namespace ares::GDB {

enum class Signal : u8 {
  HANGUP  = 1,
  INT     = 2,
  QUIT    = 3,
  ILLEGAL = 4,
  TRAP    = 5,
  ABORT   = 6,
  SEGV    = 11,
};

/**
 * GDB based debugging server.
 * This allows for remote debugging over TCP.
 */
class Server : public nall::TCPText::Server {
  public:

    auto reset() -> void;

    struct {
      // Memory
      function<string(u64 address, u32 unitCount)> read{};
      function<void(u64 address, u32 unitSize, u64 value)> write{};
      function<u64(u64 address)> normalizeAddress{};

      // Registers
      function<string()> regReadGeneral{};
      function<void(const string &regData)> regWriteGeneral{};
      function<string(u32 regIdx)> regRead{};
      function<bool(u32 regIdx, u64 regValue)> regWrite{};

      // Emulator
      function<void(u64 address)> emuCacheInvalidate{};
      function<string()> targetXML{};


    } hooks{};

    // Exception
    auto reportSignal(Signal sig, u64 originPC) -> bool;

    // PC / Memory State Updates
    auto reportPC(u64 pc) -> bool;
    auto reportMemRead(u64 address, u32 size) -> void;
    auto reportMemWrite(u64 address, u32 size) -> void;

    // Breakpoints / Watchpoints
    auto isHalted() const { return forceHalt && haltSignalSent; }
    auto hasBreakpoints() const { 
      return breakpoints || singleStepActive || watchpointRead || watchpointWrite;
    }
    
    auto getPcOverride() const { return pcOverride; };

    auto updateLoop() -> void;
    auto getStatusText(u32 port, bool useIPv4) -> string;

  protected:
    auto onText(string_view text) -> void override;
    auto onConnect() -> void override;
    auto onDisonnect() -> void override;

  private:
    bool insideCommand{false};
    string cmdBuffer{""};

    bool haltSignalSent{false}; // marks if a signal as been sent for new halts (force-halt and breakpoints)
    bool forceHalt{false}; // forces a halt despite no breakpoints being hit
    bool singleStepActive{false};

    bool nonStopMode{false}; // (NOTE: Not working for now), gets set if gdb wants to switch over to async-messaging
    bool handshakeDone{false}; // set to true after a few handshake commands, used to prevent exception-reporting until client is ready
    bool requestDisconnect{false}; // set to true if the client decides it wants to disconnect

    bool hasActiveClient{false};
    u32 messageCount{0}; // message count per update loop
    s32 currentThreadC{-1}; // selected thread for the next 'c' command

    u64 currentPC{0};
    maybe<u64> pcOverride{0}; // temporary override to handle edge-cases for exceptions/watchpoints

    // client-state:
    vector<u64> breakpoints{}; // prefer vector for data-locality
    vector<Watchpoint> watchpointRead{};
    vector<Watchpoint> watchpointWrite{};

    auto processCommand(const string& cmd, bool &shouldReply) -> string;
    auto resetClientData() -> void;

    auto reportWatchpoint(const Watchpoint &wp, u64 address) -> void;

    auto sendPayload(const string& payload) -> void;
    auto sendSignal(Signal code) -> void;
    auto sendSignal(Signal code, const string& reason) -> void;

    auto haltProgram() -> void;
    auto resumeProgram() -> void;
};

extern Server server;

}