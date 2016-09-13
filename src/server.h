#pragma once

#include "common.h"

namespace server {

	// Derived from: https://github.com/Smartype/crow/commit/5a630b20cb91d180a1a4cea98457a5a50d7d2677
	struct DocumentMiddleWare
	{
		std::string document_root;

		struct context
		{
		};

		DocumentMiddleWare() : document_root{ "." }
		{
		}

		void setDocumentRoot(const std::string& doc)
		{
			document_root = doc;
		}

		void before_handle(crow::request& req, crow::response& res, context& ctx)
		{
		}

		void after_handle(crow::request& req, crow::response& res, context& ctx)
		{
			if (res.code != 404) {
				return;
			}

			const std::string spath{ "../www" + req.url };
			std::ifstream inf{ std::cref(spath) };
			if (!inf)
			{
				CROW_LOG_DEBUG << "failed to read file";
				res.code = 400;
				return;
			}

			std::string body{ std::istreambuf_iterator<char>(inf), std::istreambuf_iterator<char>() };
			res.code = 200;
			res.body = body;
			res.set_header("Cache-Control", "public, max-age=86400");
			
			std::string ext = spath.substr(spath.rfind('.')+1);
			boost::algorithm::to_lower(ext);
			static std::unordered_map<std::string, std::string> mimeTypes = {
				{ "html", "text/html; charset=utf-8" },
				{ "htm", "text/html; charset=utf-8" },
				{ "js", "application/javascript; charset=utf-8" },
				{ "css", "text/css; charset=utf-8" },
				{ "gif", "image/gif" },
				{ "png", "image/png" },
			};

			const auto& mime = mimeTypes.find(ext);
			if (mime != mimeTypes.end())
			{
				res.set_header("Content-Type", mime->second);
			}
		}
	};

	boost::thread Init();

	void Run();

}