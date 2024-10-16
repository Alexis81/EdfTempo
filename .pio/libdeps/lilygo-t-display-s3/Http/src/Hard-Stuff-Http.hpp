#pragma once

// libs
#include <TimeLib.h>
#include <ArduinoHttpClient.h>

#ifndef HTTP_MAX_HEADERS
#define HTTP_MAX_HEADERS 10
#endif

#pragma region HTTP_STRUCTS
struct KeyValuePair
{
    String key = "";
    String value = "";
};

struct HardStuffHttpRequest
{
    KeyValuePair headers[HTTP_MAX_HEADERS]; // Request headers
    KeyValuePair params[HTTP_MAX_HEADERS];  // Request headers
    int header_count = 0;                   // Number of headers in request
    int param_count = 0;                    // Number of params in request
    String content = "";                    // Request content

    /**
     * @brief Add a header to the request in a key: value fashion
     */
    void addHeader(const String &key, const String &value)
    {
        if (header_count < HTTP_MAX_HEADERS)
        {
            headers[header_count].key = key;
            headers[header_count].value = value;
            header_count++;
        }
        else
            Serial.println("MAX HEADERS REACHED!");
    }

    /**
     * @brief Add a header to the request in a key: value fashion
     */
    void addParam(const String &key, const String &value)
    {
        if (param_count < HTTP_MAX_HEADERS)
        {
            params[param_count].key = key;
            params[param_count].value = value;
            param_count++;
        }
        else
            Serial.println("MAX HEADERS REACHED!");
    }

    /**
     * @brief Clear the contents of the HTTP request
     *
     * @param clear_all
     */
    void clear(bool ignore_headers = false)
    {
        content = ""; // Clear the content

        if (!ignore_headers)
        {
            // Reset headers and header count
            for (int i = 0; i < header_count; ++i)
            {
                headers[i].key = "";
                headers[i].value = "";
            }
            header_count = 0;
        }

        // Reset headers and header count
        for (int i = 0; i < param_count; ++i)
        {
            params[i].key = "";
            params[i].value = "";
        }
        param_count = 0;
    }

    /**
     * @brief Print the HardStuffHttpRequest to serial (useful for debugging or analysing)
     *
     * @param output_stream Override the stream, e.g. if dumping to an FS file
     */
    void print(Stream *output_stream = &Serial)
    {
        output_stream->println("Headers:");
        for (int i = 0; i < header_count; i++)
        {
            output_stream->println(headers[i].key + " : " + headers[i].value);
        }
        output_stream->println("Content:");
        output_stream->println(content);
    }
};

struct HardStuffHttpResponse
{
    int status_code = 0;                    // HTTP status code
    KeyValuePair headers[HTTP_MAX_HEADERS]; // Response headers
    int header_count = 0;                   // Number of headers in response
    String body = "";                       // Response body
    int content_length = 0;                 // Content length
    bool is_chunked = false;                // Flag for chunked response

    // Quick-check if the status code was between 200 and 300
    bool success() const { return status_code >= 200 && status_code < 300; }

    /**
     * @brief Print the HardStuffHttpResponse to serial (useful for debugging or analysing)
     *
     * @param output_stream Override the stream, e.g. if dumping to an FS file
     */
    void print(Stream *output_stream = &Serial) const
    {
        output_stream->println("Response status code: " + String(status_code));
        output_stream->println(F("Response Headers:"));
        for (int i = 0; i <= header_count; i++)
            output_stream->println("    " + headers[i].key + " : " + headers[i].value);
        output_stream->println("Content length: " + String(content_length));
        output_stream->println(F("Response:"));
        output_stream->println(body);
        output_stream->println("Response is " + String(is_chunked ? "" : "not ") + "chunked.");
        output_stream->println("Body length is: " + String(body.length()));
    }

    /**
     * @brief Clear the contents of the HTTP response (should you wish to recycle the response element)
     */
    void clear()
    {
        status_code = 0;           // HTTP status code
        headers[HTTP_MAX_HEADERS]; // Response headers
        header_count = 0;          // Number of headers in response
        body = "";                 // Response body
        content_length = 0;        // Content length
        is_chunked = false;        // Flag for chunked response

        // Reset headers and header count
        for (int i = 0; i < header_count; ++i)
        {
            headers[i].key = "";
            headers[i].value = "";
        }
        header_count = 0;
    }
};
#pragma endregion
class HardStuffHttpClient : public HttpClient
{
public:
    // HttpClient http(HTTP_UNDERLYING_CLIENT, HTTP_SERVER, HTTP_PORT);
    // Implementation of the constructor for HardStuffHttpClient
    HardStuffHttpClient(Client &underlying_client, const char *server_name, uint16_t server_port) : HttpClient(underlying_client, server_name, server_port){};

    /**
     * @brief Post some content string to a given endpoint of the attached server
     *
     * @param endpoint Given endpoint, e.g. "/device_1/shadow"
     * @param request A request object that contains contents and headers
     * @return HardStuffHttpResponse, the response as an HardStuffHttpResponse object
     */
    HardStuffHttpResponse postToHTTPServer(String endpoint, HardStuffHttpRequest *request)
    {
        HardStuffHttpResponse response;
        // Post the HTTP request

        for (int i = 0; i < request->param_count; i++)
        {
            endpoint += i == 0 ? "?" : "&";
            if (request->params[i].key)
                endpoint += request->params[i].key + "=" + request->params[i].value;
        }

        this->beginRequest();
        this->post(endpoint);
        for (int i = 0; i < request->header_count; i++)
        {
            this->sendHeader(request->headers[i].key, request->headers[i].value);
        }
        Serial.println("B");
        this->sendHeader("Content-Length", request->content.length());
        this->beginBody();
        // this->println(request->content);
        this->println(request->content);
        this->endRequest();

        if (this->getWriteError() != 0)
        {
            response.status_code = HTTP_ERROR_TIMED_OUT;
            this->stop();
            return response;
        }

        Serial.print(F("Performing HTTP POST request... "));

        response.status_code = this->responseStatusCode();
        for (int i = 0; i < HTTP_MAX_HEADERS; i++)
        {
            if (this->headerAvailable())
            {
                response.headers[response.header_count].key = this->readHeaderName();
                response.headers[response.header_count].value = this->readHeaderValue();
                response.header_count = min(response.header_count + 1, HTTP_MAX_HEADERS);
            }
            else
            {
                break;
            }
        }
        if (this->headerAvailable())
        {
            Serial.println("MAX HEADERS REACHED!");
        }
        this->skipResponseHeaders();
        response.body = this->responseBody();
        this->stop();
        return response;
    }

    /**
     * @brief Get the whatever contents from a given endpoint of the attached server
     *
     * @param endpoint Given endpoint, e.g. "/version"
     * @param request
     * @param skip_body Rarely used, only in circumstances where you want to stream the body response somewhere else (other than a string)
     * @return HardStuffHttpResponse
     */
    HardStuffHttpResponse getFromHTTPServer(String endpoint, HardStuffHttpRequest *request = nullptr, bool skip_body = false)
    {
        HardStuffHttpResponse response;

        if (request != nullptr)
        {
            for (int i = 0; i < request->param_count; i++)
            {
                endpoint += i == 0 ? "?" : "&";
                if (request->params[i].key)
                {
                    endpoint += request->params[i].key + "=" + request->params[i].value;
                }
            }
        }
        // Get
        this->beginRequest();
        this->get(endpoint);

        if (this->getWriteError() != 0)
        {
            response.status_code = HTTP_ERROR_TIMED_OUT;
            this->stop();
            return response;
        }
        if (request != nullptr)
        {
            for (int i = 0; i < request->header_count; i++)
            {
                this->sendHeader(request->headers[i].key, request->headers[i].value);
            }
            if (request->content.length())
            {
                this->sendHeader("Content-Length", request->content.length());
                this->beginBody();
                this->println(request->content);
            }
        }
        this->endRequest();
        // Serial.print(F("Performing HTTP GET request... "));

        response.status_code = this->responseStatusCode();
        for (int i = 0; i < HTTP_MAX_HEADERS; i++)
        {
            if (this->headerAvailable())
            {
                response.headers[response.header_count].key = this->readHeaderName();
                response.headers[response.header_count].value = this->readHeaderValue();
                response.header_count = min(response.header_count + 1, HTTP_MAX_HEADERS);
            }
            else
            {
                break;
            }
        }
        if (this->headerAvailable())
        {
            Serial.println("MAX HEADERS REACHED!");
        }
        response.content_length = this->contentLength();
        response.is_chunked = this->isResponseChunked();
        if (!skip_body)
        {
            response.body = this->responseBody();
            this->stop();
        }
        return response;
    }

public:
    /**
     * @brief Format a time_t value into an ISO8601 String
     * ISO8601 is, YYYY-MM-DDThh:mm:ssZ
     */
    String formatTimeISO8601(time_t t)
    {
        char buffer[25];

        // Break down time_t into its components
        int Year = year(t);
        int Month = month(t);
        int Day = day(t);
        int Hour = hour(t);
        int Minute = minute(t);
        int Second = second(t);

        // Format the string in ISO 8601 format
        // Note: This assumes UTC time. Adjust accordingly if using local time.
        snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
                 Year, Month, Day, Hour, Minute, Second);

        return String(buffer);
    }

    /**
     * @brief Format an ISO8601 String into a time_t value
     * ISO8601 is, YYYY-MM-DDThh:mm:ssZ
     */
    time_t formatTimeFromISO8601(String timestamp)
    {
        int Year, Month, Day, Hour, Minute, Second;
        sscanf(timestamp.c_str(), "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
               &Year, &Month, &Day, &Hour, &Minute, &Second);
        tmElements_t tm;
        tm.Year = Year - 1970; // Adjust year
        tm.Month = Month;      // TODO: Adjust month
        tm.Day = Day;
        tm.Hour = Hour;
        tm.Minute = Minute;
        tm.Second = Second;
        return makeTime(tm); // Convert to time_t
    }
};