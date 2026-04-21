#ifndef _GEMINI_CONFIG_H
#define _GEMINI_CONFIG_H

// DO NOT CHECK IN THE FOLLOWING LINE
#define GEMINI_API_KEY ""

#define GEMINI_BASE_URL "https://generativelanguage.googleapis.com"
#define GEMINI_URL GEMINI_BASE_URL "/v1beta/models/gemini-3.1-flash-lite-preview:generateContent"
#define GEMINI_FILE_INITIATE_URL GEMINI_BASE_URL "/upload/v1beta/files"


#define SYSTEM_INSTRUCTION "You are a first-year university engineering student. Transcribe the following recordings, and tell me if they make sense to you from a student's perspective."

#endif