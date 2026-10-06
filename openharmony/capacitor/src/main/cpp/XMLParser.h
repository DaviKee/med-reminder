/*
 * Copyright (c) 2025 Huawei Device, Inc. Ltd. and <马弓手>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef XML_PARSER_H
#define XML_PARSER_H

#include <string>
#include "XMLNode.h"

/**
 * @brief parse xml from file or memory
 */
class CXMLParser {
public:
    CXMLParser() {}

    ~CXMLParser() {}

    /**
     * @brief parse xml from memory
     * @param input:std::string of memory
     * @return bool:true for success or false failed.
     */
    bool parse(const char* in);

    /**
     * @brief parse xml from file
     * @param fname:name of filepath
     * @return bool:true for success or false failed.
     */
    bool parseFile(const char* fileName);

    /**
     * @brief Gets the top-level node
     * @return SXMLNode*:Non-null if successful, null if failed.
     */
    SXMLNode* getTopNode();

    /**
     * @brief Gets node from path
     * @param path：path
     * @return Non-null if successful, null if failed
     */
    SXMLNode* getNode(const std::string& path);

    /**
     * @brief Gets the text content of a node
     * @param path:Node path
     * @param index:Node index number
     * @return std::string:Returns the node text content
     */
    std::string getContentText(const std::string& path, int index = 0);

    /**
     * @brief Gets ErrorInfo
     * @return std::string:Returns the std::string error
     */
    const std::string& getErrorInfo() const
    {
        return m_errorInfo;
    }

    /**
     * @brief Gets the error line number
     * @return int:Returns the error line number
     */
    const int getErrorLine() const
    {
        return errorLine;
    }

    /**
     * Gets the text content of a node
     * @param[in] path:Node path
     * @param[out] content:the text content of a node
     * @param[in] index:Node index number
     * @return bool:true for success or false failed
     */
    bool getContentText(const std::string& path, std::string& content, int index = 0);

private:
    SXMLNode m_topNode;
    std::string m_errorInfo;
    int errorLine{0};
    std::string getNodeName(char** in);
    std::string getAttrName(char** input);
    bool getAttrValue(const char* name, char** in, std::string& strValue);
    std::string getRemark(char** in);
    std::string getCData(char** in);
    std::string getDocType(char** in);
    std::string getFunction(char** in);
    bool getNodeAttrs(char** in, SXMLNode* node);
};

#endif
