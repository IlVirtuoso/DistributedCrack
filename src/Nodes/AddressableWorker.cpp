#include "Nodes/AddressableWorker.hpp"
#include <boost/mpi/collectives.hpp>
#include "OptionsBag.hpp"
#include "Schema.hpp"
using namespace boost::mpi;

void AddressableWorker::Routine()
{
	bool terminate = false;
	std::vector<size_t> address{};
	std::string* result = new std::string();
	std::vector<std::string> chunk{};
	_requests.push_back(_communicator.irecv(0, MESSAGE, address));
	while (!terminate)
	{
		auto req = wait_any(_requests.begin(), _requests.end());
		switch (req.first.tag())
		{
			case MESSAGE:
			_generator->AssignAddress(address[0]);
			chunk = _generator->generateChunk(address[1]);
			if (Compute(chunk, result))
			{
				_communicator.send(0, FOUND, *result);
				terminate = true;
			}
			else
			{
				_requests.push_back(_communicator.irecv(0, MESSAGE, address));
				_communicator.send(0, WORK);
			}
			break;

			case TERMINATE:
			terminate = true;
			break;
			default:
			break;
		}
		DeleteRequest(req.second.base());
	}
}

void AddressableWorker::Initialize()
{
	_logger->TraceInformation() << "Broadcasting target" << std::endl;
	broadcast(_communicator, _target, 0);
}

void AddressableWorker::OnBeginRoutine()
{
	int startSequence = 4;
	broadcast(_communicator, startSequence, 0);
	_generator = new AssignedSequenceGenerator(startSequence);
	_communicator.send(0, WORK);
}

void AddressableWorker::OnEndRoutine()
{
}
