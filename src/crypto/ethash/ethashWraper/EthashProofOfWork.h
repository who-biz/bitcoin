#pragma once

/// Proof of work definition for Ethash.
struct EthashProofOfWork
{
    struct Result
	{
		uint256 value;
		uint256 mixHash;
		std::string msg;
	};
};