Einsteinium Core integration/staging tree
=====================================

[![Build Status](https://travis-ci.org/litecoin-project/litecoin.svg?branch=master)](https://travis-ci.org/litecoin-project/litecoin) //add our own build status here//

https://emc2.foundation

//Temporary Explanation of the Repo//

This Repository is currently in work-on state, building the gateway to upstream developments in the Litecoin Project. It will be renamed at the time of the first release.

What is Einsteinium?
----------------

Einsteinium is an experimental digital currency that enables instant payments to
anyone, anywhere in the world. Einsteinium uses peer-to-peer technology to operate
with no central authority: managing transactions and issuing money are carried
out collectively by the network. Einsteinium Core is the name of open source
software which enables the use of this currency.

- Algorithm: Scrypt PoW
- Total Einsteinium: 299,792,458
- Initial block value: 1024
- Reward Reduction Method: Block Halving
- Block Target Time: 60 seconds
- Difficulty Re-targeting: per block (Kimoto Gravity Well)
- RPC Port: 41879
- P2P Port: 41878
- Donation to the Einsteinium Charitable Fund (per block): 2%
- Donation to to faucets, give-aways and marketing (per block): 0.5%

For more information, as well as an immediately useable, binary version of
the Einsteinium Core software, see [https://emc2.foundation](https://emc2.foundation).

License
-------

Einsteinium Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/HashUnlimited/Einsteinium-Core-Working-Repo/tags) //ToDo: change after first release //are created
regularly to indicate new official, stable release versions of Einsteinium Core.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).

The developer [mailing list](https://add mailing list link)
should be used to discuss complicated or controversial changes before working
on a patch set.

Developers can easily be reached on our [Slack](http://emc2slack.herokuapp.com/).

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](/doc/unit-tests.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`

There are also [regression and integration tests](/qa) of the RPC interface, written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/qa) are installed) with: `qa/pull-tester/rpc-tests.py`

The Travis CI system makes sure that every pull request is built for Windows, Linux, and OS X, and that unit/sanity tests are run automatically.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

Translations
------------

We only accept translation fixes that are submitted through [Bitcoin Core's Transifex page](https://www.transifex.com/projects/p/bitcoin/).
Translations are converted to Einsteinium periodically.

Translations are periodically pulled from Transifex and merged into the git repository. See the
[translation process](doc/translation_process.md) for details on how this works.

**Important**: We do not accept translation changes as GitHub pull requests because the next
pull from Transifex would automatically overwrite them again.
