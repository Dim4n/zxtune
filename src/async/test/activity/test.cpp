#include <async/activity.h>
#include <boost/thread/thread.hpp>
#include <boost/make_shared.hpp>
#include <iostream>

#define FILE_TAG 238D7960

//fix for new boost versions
namespace boost
{
  void tss_cleanup_implemented() { }
}

namespace
{
	void ShowError(unsigned /*level*/, Error::LocationRef loc, Error::CodeType code, const String& text)
	{
		std::cout << Error::AttributesToString(loc, code, text);
	}
}

namespace
{
	using namespace Async;
	
	Error FailedToPrepareError()
  {
    return Error(THIS_LINE, 1, "Failed to prepare");
  }

	Error FailedToExecuteError()
  {
    return Error(THIS_LINE, 2, "Failed to execute");
  }
	
	class InvalidOperation : public Operation
	{
	public:
		virtual Error Prepare()
		{
			return FailedToPrepareError();
		}
		
		virtual Error Execute()
		{
			throw Error(THIS_LINE, 3, "Should not be called");
		}
	};

	class ErrorResultOperation : public Operation
	{
	public:
		virtual Error Prepare()
		{
			return Error();
		}
		
		virtual Error Execute()
		{
			return FailedToExecuteError();
		}
	};

  class LongOperation : public Operation
  {
  public:
    virtual Error Prepare()
    {
      return Error();
    }

    virtual Error Execute()
    {
      std::cout << "    start long activity" << std::endl;
      boost::this_thread::sleep(boost::posix_time::milliseconds(2000));
      std::cout << "    end long activity" << std::endl;
      return Error();
    }
  };

	void TestInvalidActivity()
	{
		std::cout << "Test for invalid activity" << std::endl;
		Activity::Ptr result;
		if (const Error& err = Activity::Create(boost::make_shared<InvalidOperation>(), result))
		{
			if (err != FailedToPrepareError())
			{
				throw Error(THIS_LINE, 4, "Invalid error returned").AddSuberror(err);
			}
			std::cout << "Succeed\n";
		}
		else
		{
			throw Error(THIS_LINE, 5, "Should not create activity");
		}
	}
	
	void TestActivityErrorResult()
	{
		std::cout << "Test for valid activity error result" << std::endl;
		Activity::Ptr result;
		ThrowIfError(Activity::Create(boost::make_shared<ErrorResultOperation>(), result));
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    if (result->IsExecuted())
    {
      throw Error(THIS_LINE, 6, "Activity should be finished");
    }
		if (const Error& err = result->Wait())
		{
			if (err != FailedToExecuteError())
			{
				throw Error(THIS_LINE, 7, "Invalid error returned").AddSuberror(err);
			}
			std::cout << "Succeed\n";
		}
		else
		{
			throw Error(THIS_LINE, 8, "Activity should not finish successfully");
		}
	}

	void TestLongActivity()
  {
    std::cout << "Test for valid long activity" << std::endl;
    Activity::Ptr result;
    ThrowIfError(Activity::Create(boost::make_shared<LongOperation>(), result));
    if (!result->IsExecuted())
    {
      throw Error(THIS_LINE, 9, "Activity should be executed now");
    }
    ThrowIfError(result->Wait());
    if (result->IsExecuted())
    {
      throw Error(THIS_LINE, 10, "Activity is still executed");
    }
  }
}

int main()
{
  try
  {
		TestInvalidActivity();
		TestActivityErrorResult();
    TestLongActivity();
  }
  catch (const Error& err)
  {
		std::cout << "Failed: \n";
		err.WalkSuberrors(ShowError);
  }
}