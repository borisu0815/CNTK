//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

namespace Microsoft { namespace MSR { namespace CNTK {

class hdfsFile {
private:
    std::wstring m_filename;
    FILE* m_file;        // file handle
    bool m_pcloseNeeded; // was opened with popen(), use pclose() when destructing
    bool m_seekable;     // this stream is seekable
    int m_options;       // FileOptions ored togther
    void Init(const wchar_t* filename, int fileOptions);

public:
    hdfsFile(const std::string&  filename, int fileOptions);
    ~hdfsFile();

    void Flush();

    bool CanSeek() const { return m_seekable; }
    size_t Size();
    uint64_t GetPosition();
    void SetPosition(uint64_t pos);
    void SkipToDelimiter(int delim);

    bool IsTextBased();

    bool IsUnicodeBOM(bool skip = false);
    bool IsEOF();
    bool IsWhiteSpace(bool skip = false);
    int EndOfLineOrEOF(bool skip = false);
    int Setvbuf();

    // TryGetText - for text value, try and get a particular type
    // returns - true if value returned, otherwise false, can't parse
    template <typename T>
    bool TryGetText(T& val)
    {
        assert(IsTextBased());
        return !!ftrygetText(m_file, val);
    }

    void GetLine(std::string& str);
    void GetLines(std::vector<std::wstring>& lines);
    void GetLines(std::vector<std::string>& lines);

    // static helpers
    // test whether a file exists
    template<class String>
    static bool Exists(const String& filename);

    // make intermediate directories
    template<class String>
    static void MakeIntermediateDirs(const String& filename);

    // determine the directory and naked file name for a given pathname
    static std::wstring DirectoryPathOf(std::wstring path);
    static std::wstring FileNameOf(std::wstring path);

    // get path of current executable
    static std::wstring GetExecutablePath();

    // put operator for basic types
    template <typename T>
    File& operator<<(T val)
    {
        {
            if (IsTextBased())
                fputText(m_file, val);
            else
                fput(m_file, val);
        }
        return *this;
    }
    File& operator<<(const std::wstring& val);
    File& operator<<(const std::string& val);
    File& operator<<(FileMarker marker);
    File& PutMarker(FileMarker marker, size_t count);
    File& PutMarker(FileMarker marker, const std::string& section);
    File& PutMarker(FileMarker marker, const std::wstring& section);

    // put operator for vectors of types
    template <typename T>
    File& operator<<(const std::vector<T>& val)
    {
        this->PutMarker(fileMarkerBeginList, val.size());
        for (int i = 0; i < val.size(); i++)
        {
            *this << val[i] << fileMarkerListSeparator;
        }
        *this << fileMarkerEndList;
        return *this;
    }

    // get operator for basic types
    template <typename T>
    File& operator>>(T& val)
    {
        if (IsTextBased())
            fgetText(m_file, val);
        else
            fget(m_file, val);
        return *this;
    }

    void WriteString(const char* str, int size = 0);                   // zero terminated strings use size=0
    void ReadString(char* str, int size);                              // read up to size bytes, or a zero terminator (or space in text mode)
    void WriteString(const wchar_t* str, int size = 0);                // zero terminated strings use size=0
    void ReadString(wchar_t* str, int size);                           // read up to size bytes, or a zero terminator (or space in text mode)
    void ReadChars(std::string& val, size_t cnt, bool reset = false);  // read a specified number of characters, and reset read pointer if requested
    void ReadChars(std::wstring& val, size_t cnt, bool reset = false); // read a specified number of characters, and reset read pointer if requested

    File& operator>>(std::wstring& val);
    File& operator>>(std::string& val);
    File& operator>>(FileMarker marker);
    File& GetMarker(FileMarker marker, size_t& count);
    File& GetMarker(FileMarker marker, const std::string& section);
    File& GetMarker(FileMarker marker, const std::wstring& section);
    bool TryGetMarker(FileMarker marker, const std::string& section);
    bool TryGetMarker(FileMarker marker, const std::wstring& section);

    bool IsMarker(FileMarker marker, bool skip = true);

    // get a vector of types
    template <typename T>
    File& operator>>(std::vector<T>& val)
    {
        T element;
        val.clear();
        size_t size = 0;
        this->GetMarker(fileMarkerBeginList, size);
        if (size > 0)
        {
            for (int i = 0; i < size; i++)
            {
                // get list separators if not the first element
                if (i > 0)
                    *this >> fileMarkerListSeparator;
                *this >> element;
                val.push_back(element);
            }
            *this >> fileMarkerEndList;
        }
        else
        {
            bool first = true;
            while (!this->IsMarker(fileMarkerEndList))
            {
                if (!first)
                    *this >> fileMarkerListSeparator;
                *this >> element;
                val.push_back(element);
                first = false;
            }
        }
        return *this;
    }

    operator FILE*() const { return m_file; }

    // Read a matrix stored in text format from 'filePath' (whitespace-separated columns, newline-separated rows),
    // and return a flat vector containing the contents of this file in column-major format.
    // filePath: path to file containing matrix in text format.
    // numRows/numCols: after this function is called, these parameters contain the number of rows/columns in the matrix.
    // returns: a flat array containing the contents of this file in column-major format
    // This function does not quite fit here, but it fits elsewhere even worse. TODO: change to use File class!
    template <class ElemType>
    static std::vector<ElemType> LoadMatrixFromTextFile(const std::wstring& filePath, size_t& /*out*/ numRows, size_t& /*out*/ numCols);
    // same as LoadMatrixFromTextFile() but from a string literal
    // This function fits even less...
    template <class ElemType>
    static std::vector<ElemType> LoadMatrixFromStringLiteral(const std::string& literal, size_t& /*out*/ numRows, size_t& /*out*/ numCols);

    // Read a label file.
    // A label file is a sequence of text lines with one token per line, where each line maps a string to an index, starting with 0.
    // This function allows spaces inside the word name, but trims surrounding spaces.
    // TODO: Move this to class File, as this is similar in nature to LoadMatrixFromTextFile().
    template <class LabelType>
    static void LoadLabelFile(const std::wstring& filePath, std::vector<LabelType>& retLabels)
    {
        File file(filePath, fileOptionsRead | fileOptionsText);

        LabelType str;
        retLabels.clear();
        while (!file.IsEOF())
        {
            file.GetLine(str);
            if (str.empty())
                if (file.IsEOF())
                    break;
                else
                    RuntimeError("LoadLabelFile: Invalid empty line in label file.");

            retLabels.push_back(trim(str));
        }
    }
};

}}}
