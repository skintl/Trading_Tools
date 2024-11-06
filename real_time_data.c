#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#define API_KEY "cslv5fpr01qgp6njmf2gcslv5fpr01qgp6njmf30"

// Structure to store API response
struct MemoryStruct {
    char *memory;
    size_t size;
};

// Function to handle API response
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// Function to plot data using gnuplot
void plotData(const char *filename) {
    FILE *gnuplotPipe = popen("gnuplot -persistent", "w");
    if (gnuplotPipe) {
        fprintf(gnuplotPipe, "set title 'Stock Prices Over Time'\n");
        fprintf(gnuplotPipe, "set xlabel 'Date'\n");
        fprintf(gnuplotPipe, "set ylabel 'Price'\n");
        fprintf(gnuplotPipe, "set xdata time\n");
        fprintf(gnuplotPipe, "set timefmt '%%Y-%%m-%%d'\n");
        fprintf(gnuplotPipe, "set format x '%%Y-%%m-%%d'\n");
        fprintf(gnuplotPipe, "set xtics rotate\n");
        fprintf(gnuplotPipe, "plot '%s' using 1:2 with lines title 'Stock Price'\n", filename);
        pclose(gnuplotPipe);
    } else {
        printf("Error: Could not open gnuplot.\n");
    }
}

int main() {
    CURL *curl;
    CURLcode res;

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    char symbol[20];
    char timeFrame[20];

    // Get the ticker symbol and time frame from the user
    printf("Enter the stock ticker symbol: ");
    scanf("%s", symbol);
    printf("Enter the time frame (e.g., 1min, 5min, 15min, daily, weekly, monthly): ");
    scanf("%s", timeFrame);

    // Construct the API URL
    char url[512];
    snprintf(url, sizeof(url), "https://www.alphavantage.co/query?function=TIME_SERIES_%s&symbol=%s&apikey=%s", 
             strcmp(timeFrame, "weekly") == 0 ? "WEEKLY" : "DAILY", symbol, API_KEY);

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            // Parse JSON response
            cJSON *json = cJSON_Parse(chunk.memory);
            if (json == NULL) {
                printf("Error parsing JSON\n");
            } else {
                // Write data to a file
                FILE *dataFile = fopen("stock_data.txt", "w");
                if (dataFile) {
                    cJSON *timeSeries = cJSON_GetObjectItemCaseSensitive(json, 
                        strcmp(timeFrame, "weekly") == 0 ? "Weekly Time Series" : "Time Series (Daily)");
                    if (timeSeries) {
                        cJSON *date;
                        cJSON_ArrayForEach(date, timeSeries) {
                            cJSON *price = cJSON_GetObjectItemCaseSensitive(date, "4. close");
                            if (cJSON_IsString(price)) {
                                fprintf(dataFile, "%s %s\n", date->string, price->valuestring);
                            }
                        }
                    }
                    fclose(dataFile);

                    // Plot the data
                    plotData("stock_data.txt");
                } else {
                    printf("Error: Could not write to file.\n");
                }
                cJSON_Delete(json);
            }
        }

        curl_easy_cleanup(curl);
        free(chunk.memory);
    }
    curl_global_cleanup();

    return 0;
}

