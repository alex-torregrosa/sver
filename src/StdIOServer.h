#include <iostream>

#include "LibLsp/JsonRpc/Condition.h"
#include "LibLsp/lsp/general/initialize.h"
#include "LibLsp/lsp/ProtocolJsonHandler.h"
#include "LibLsp/lsp/general/exit.h"
#include "LibLsp/lsp/textDocument/declaration_definition.h"

#include "LibLsp/JsonRpc/Endpoint.h"
#include "LibLsp/JsonRpc/RemoteEndPoint.h"
#include "LibLsp/JsonRpc/stream.h"

class DummyLog :public lsp::Log
{
public:

	void log(Level level, std::wstring&& msg)
	{

		std::wcerr << msg << std::endl;
	};
	void log(Level level, const std::wstring& msg)
	{
		std::wcerr << msg << std::endl;
	};
	void log(Level level, std::string&& msg)
	{
		std::cerr << msg << std::endl;
	};
	void log(Level level, const std::string& msg)
	{
		std::cerr << msg << std::endl;
	};
};


class StdIOServer
{
public:
	
	StdIOServer() : remote_end_point_(protocol_json_handler, endpoint, _log)
	{
		remote_end_point_.registerHandler([&](const td_initialize::request& req)
		{
				td_initialize::response rsp;
				rsp.id = req.id;
				CodeLensOptions code_lens_options;
				code_lens_options.resolveProvider = true;
				rsp.result.capabilities.codeLensProvider = code_lens_options;
				return rsp;
		});

		remote_end_point_.registerHandler([&](Notify_Exit::notify& notify)
			{
				remote_end_point_.Stop();
				esc_event.notify(std::make_unique<bool>(true));
			});
		remote_end_point_.registerHandler([&](const td_definition::request& req,
			const CancelMonitor& monitor)
			{
				td_definition::response rsp;
				rsp.result.first = std::vector<lsLocation>();
				if (monitor && monitor())
				{
					_log.info("textDocument/definition request had been cancel.");
				}
				return rsp;
			});

		remote_end_point_.startProcessingMessages(input, output);
	}
	~StdIOServer()
	{
		
	}

	struct ostream : lsp::base_ostream<std::ostream>
	{
		explicit ostream(std::ostream& _t)
			: base_ostream<std::ostream>(_t)
		{
			
		}

		std::string what() override
		{
			return {};
		}
	};
	struct istream :lsp::base_istream<std::istream>
	{
		explicit istream(std::istream& _t)
			: base_istream<std::istream>(_t)
		{
		}

		std::string what() override
		{
			return {};
		}
	};
	std::shared_ptr < lsp::ProtocolJsonHandler >  protocol_json_handler = std::make_shared < lsp::ProtocolJsonHandler >();
	DummyLog _log;
	
	std::shared_ptr<ostream> output = std::make_shared<ostream>(std::cout);
	std::shared_ptr<istream> input = std::make_shared<istream>(std::cin);

	std::shared_ptr < GenericEndpoint >  endpoint = std::make_shared<GenericEndpoint>(_log);
	RemoteEndPoint remote_end_point_;
	Condition<bool> esc_event;
};
