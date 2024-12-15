#include "Emulator.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <tuple>

Chip8::Chip8() { Reset(); }

void Chip8::InitializeMemory() {
  _memory = {};

  std::copy(FONT_SET.begin(), FONT_SET.end(),
            _memory.begin() + MEMORY_OFFSET_FONT);
}

void Chip8::Reset() {
  _soundTimer = {};
  _lastExecution = std::chrono::steady_clock::time_point::min();
  InitializeMemory();
  _stack = {};
  _carry = 0;
  _registers = {};
  _programCounter = INITIAL_PROGRAM_COUNTER;
  _index = 0;
}

void Chip8::LoadProgram(const std::vector<Byte> &program) {
  std::copy(program.begin(), program.end(),
            _memory.begin() + MEMORY_OFFSET_PROGRAM);
}

Chip8::Opcodes Chip8::GetNextInstruction() {
  static constexpr int BYTE = 8;
  // NOLINTBEGIN(*-array-index, *-magic-numbers, *-signed-bitwise)
  return static_cast<Opcodes>(_memory[_programCounter] << BYTE |
                              _memory[_programCounter + 1]);
  // NOLINTEND(*-array-index, *-magic-numbers, *-signed-bitwise)
}
void Chip8::StackPush(unsigned short val) {
  if (_stack.size() == std::tuple_size_v<decltype(_stack)::container_type>) {
    throw std::runtime_error("stack overflow");
  }
  _stack.push(val);
}

unsigned short Chip8::StackPop() {
  if (_stack.empty()) {
    throw std::runtime_error("stack underflow");
  }
  const auto top = _stack.top();
  _stack.pop();
  return top;
}

void Chip8::IncrementPC() { _programCounter += 2; }

bool Chip8::ExecuteOpcode(Opcodes opcode) {
  // NOLINTBEGIN(*magic-numbers, *-signed-bitwise, *-array-index)
  constexpr auto throwOpcodeError = [](Opcodes opcode) {
    std::stringstream stream;
    stream << "Invalid opcode: " << std::hex << std::uppercase
           << std::setfill('0') << std::setw(8) << static_cast<int>(opcode);
    throw std::runtime_error(stream.str());
  };

  const auto opcodeInt = static_cast<int>(opcode);
  const auto firstFourBits = static_cast<Opcodes>(opcodeInt & 0xF000);
  if (static_cast<int>(firstFourBits) == 0) {
    const auto lastFourBits = static_cast<Opcodes>(opcodeInt & 0x000F);

    switch (lastFourBits) {
    case Opcodes::e_ADD_VX_VY: {
      const auto Y = (opcodeInt & 0x00F0) >> 4;
      const auto X = (opcodeInt & 0x0F00) >> 8;
      auto &VX = Register(X);
      const auto VY = Register(Y);
      _carry = static_cast<Byte>(bool(VX > 0xFF - VY));
      VX += VY;
      return true;
    }

    case Opcodes::e_CLEAR_SCREEN:
      _screen->Clear();
      return true;

    default:
      throwOpcodeError(opcode);
    }

  } else {

    switch (firstFourBits) {

    case Opcodes::e_SET_INDEX:
      _index = opcodeInt & 0x0FFF;
      return true;

    case Opcodes::e_CALL:
      StackPush(_programCounter);
      _programCounter = opcodeInt & 0x0FFF;
      return false;

    default:
      throwOpcodeError(opcode);
    }
  }
  // NOLINTEND(*magic-numbers, *-signed-bitwise, *-array-index)
  assert(false);
}

void Chip8::Run() {
  while (true) {
    const auto now = std::chrono::steady_clock::now();
    if (now - _lastExecution >= TICK_PERIOD) {
      _lastExecution = now;
      _soundTimer.Tick();
      auto shouldIncrementPc = ExecuteOpcode(GetNextInstruction());
      if (shouldIncrementPc) {
        IncrementPC();
      }
    }
  }
}