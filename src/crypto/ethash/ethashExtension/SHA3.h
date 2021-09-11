/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file SHA3.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 *
 * The FixedHash fixed-size "hash" container type.
 */

#pragma once

#include <string>
//#include "FixedHash.h"
#include "uint256.h"
#include "crypto/ethash/ethashExtension/Common.h"

// SHA-3 convenience routines.

/// Calculate SHA3-256 hash of the given input and load it into the given output.
//uint256 sha3(uint256 const& _input);
bool sha3(bytesConstRef _input, bytesRef o_output);

inline uint256 sha3(bytesConstRef _input) { uint256 ret; sha3(_input, bytesRef(ret.begin(), 32)); return ret; }
//inline SecureFixedHash<32> sha3Secure(bytesConstRef _input) { SecureFixedHash<32> ret; sha3(_input, ret.writable().ref()); return ret; }

/// Calculate SHA3-256 hash of the given input, returning as a 256-bit hash.
//inline uint256 sha3(bytes const& _input) { return sha3(bytesConstRef(_input.begin(), 32)); }
//inline SecureFixedHash<32> sha3Secure(bytes const& _input) { return sha3Secure(bytesConstRef(&_input)); }

/// Calculate SHA3-256 hash of the given input (presented as a binary-filled string), returning as a 256-bit hash.
inline uint256 sha3(std::string const& _input) { return sha3(bytesConstRef(_input)); }
//inline SecureFixedHash<32> sha3Secure(std::string const& _input) { return sha3Secure(bytesConstRef(_input)); }

/// Calculate SHA3-256 hash of the given input (presented as a FixedHash), returns a 256-bit hash.
inline uint256 sha3(uint256 const& _input) { return sha3( bytesConstRef(_input.begin(), 32)); }
