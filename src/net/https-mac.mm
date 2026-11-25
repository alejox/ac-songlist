// macOS: NSURLSession. Se compila con ARC (-fobjc-arc).
#include "https.hpp"

#import <Foundation/Foundation.h>

#include <memory>

namespace acsl {

HttpResult httpPostJson(const QUrl &url, const HttpHeaders &headers, const QByteArray &body, int timeoutMs)
{
	auto result = std::make_shared<HttpResult>();
	@autoreleasepool {
		NSURL *target = [NSURL URLWithString:url.toString(QUrl::FullyEncoded).toNSString()];
		if (!target) {
			result->error = QStringLiteral("direccion no valida");
			return *result;
		}
		NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:target
								       cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
								   timeoutInterval:timeoutMs / 1000.0];
		request.HTTPMethod = @"POST";
		[request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
		for (const auto &h : headers)
			[request setValue:QString::fromLatin1(h.second).toNSString() forHTTPHeaderField:QString::fromLatin1(h.first).toNSString()];
		request.HTTPBody = [NSData dataWithBytes:body.constData() length:NSUInteger(body.size())];

		NSURLSession *session = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration ephemeralSessionConfiguration]];
		dispatch_semaphore_t done = dispatch_semaphore_create(0);
		NSURLSessionDataTask *task = [session dataTaskWithRequest:request
							completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
								if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
									result->status = int([(NSHTTPURLResponse *)response statusCode]);
									result->sent = result->status > 0;
								}
								if (data && data.length > 0)
									result->body = QByteArray(static_cast<const char *>(data.bytes),
												  int(MIN(data.length, NSUInteger(1024 * 1024))));
								if (error && !result->sent)
									result->error = QString::fromNSString(error.localizedDescription);
								dispatch_semaphore_signal(done);
							}];
		[task resume];
		const bool finished =
			dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, int64_t(timeoutMs + 5000) * NSEC_PER_MSEC)) == 0;
		if (!finished) {
			[task cancel];
			[session invalidateAndCancel];
			HttpResult timedOut;
			timedOut.error = QStringLiteral("tiempo de espera agotado");
			return timedOut; // el bloque puede seguir vivo: no se toca *result
		}
		[session finishTasksAndInvalidate];
	}
	return *result;
}

bool httpsAvailable()
{
	return true;
}

} // namespace acsl
