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

#include "XMLParser.h"
#include <sys/stat.h>
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <fcntl.h>

SXMLNode* CXMLParser::getNode(const std::string& path)
{
    SXMLNode* node = &m_topNode;
    char* str = strdup(path.c_str());
    char *p, *offset = str;

    while (true) {
        if (*offset == '/') {
            offset++;
        } else {
            break;
        }
    }

    while (true) {
        if (strlen(offset) == 0) {
            break;
        }
        p = strchr(offset, '/');
        if (p != nullptr) {
            p[0] = 0;
            node = node->getNode(offset);
            offset = p + 1;
        } else {
            node = node->getNode(offset);
            break;
        }

        if (node == nullptr) {
            free(str);
            return nullptr;
        }
    }
    free(str);
    return node;
}

SXMLNode* CXMLParser::getTopNode()
{
    if (m_topNode.getNodesCount() == 0) {
        return &m_topNode;
    }
    return m_topNode.getNode(0);
}

bool CXMLParser::parseFile(const char* fileName)
{
    struct stat st;
    int size = 0;
    if (stat(fileName, &st) < 0) {
        return false;
    }
    if (st.st_size == 0) {
        return false;
    }

    char* buf = new char[st.st_size + 1];
    if (buf == nullptr) {
        return false;
    }
    int fd = open(fileName, O_RDONLY);
    if (fd < 0) {
        delete[] buf;
        return false;
    }
    size = read(fd, buf, st.st_size);
    close(fd);
    if (size <= 0) {
        delete[] buf;
        return false;
    }
    buf[size] = 0;

    bool ret = parse(buf);
    delete[] buf;
    return ret;
}

bool CXMLParser::parse(const char* in)
{
    m_topNode.freeChildElements();
    m_topNode.freeChildAttrs();

    SXMLNode* curNode = &m_topNode;
    std::string str, ctnt;
    char* input = const_cast<char*>(in);
    m_errorInfo = "";
    char c;
    int level = 0;

    while ((c = *input) != 0) {
        input++;

        if (c == '<') {
            ctnt.erase(0, ctnt.find_first_not_of(" "));
            ctnt.erase(ctnt.find_last_not_of(" ") + 1);
            if (ctnt.length() > 0) {
                SXMLDocument* content = new SXMLDocument();
                if (content == nullptr) {
                    m_topNode.freeChildElements();
                    m_topNode.freeChildAttrs();
                    return false;
                }
                content->m_strContent = ctnt;
                curNode->addElement(content);
                ctnt = "";
            }
            if (*input == '/') {
                input++;
                ctnt = "";
                std::string nm = getNodeName(&input);
                if (nm != curNode->m_strName) {
                    // errorLine = std::string::count(in, "\n", input - in) + 1;
                    m_errorInfo = "'";
                    m_errorInfo += nm + "' Cannot match name, it should be '";
                    m_errorInfo += curNode->m_strName + "'!";
                    return false;
                }
                input++;
                curNode = static_cast<SXMLNode*>(curNode->m_parent);
                if (curNode == nullptr) {
                    curNode = &m_topNode;
                }
                level--;
                continue;
            } else if (*input == '!') {
                input++;
                if (*input == '-') {
                    str = getRemark(&input);
                } else if (strncmp(input, "[CDATA[", 7) == 0) {
                    input += 7;
                    str = getCData(&input);
                    SXMLDocument* content = new SXMLDocument();
                    if (content == nullptr) {
                        m_topNode.freeChildElements();
                        m_topNode.freeChildAttrs();
                        return false;
                    }
                    content->m_strContent = str;
                    curNode->addElement(content);
                } else {
                    str = getDocType(&input);
                }
                continue;
            } else if (*input == '?') {
                input++;
                str = getFunction(&input);
                continue;
            } else {
                str = getNodeName(&input);
                SXMLNode* node = new SXMLNode();

                if (node == nullptr) {
                    m_topNode.freeChildElements();
                    m_topNode.freeChildAttrs();
                    return false;
                }
                level++;

                node->m_strName = str;
                node->m_nLevel = level;
                node->m_parent = curNode;
                curNode->addElement(node);
                curNode = node;

                if (!getNodeAttrs(&input, curNode)) {
                    return false;
                }
                continue;
            }
        }

        if (c == '/' && *input == '>') {
            ctnt = "";
            input++;
            curNode = static_cast<SXMLNode*>(curNode->m_parent);
            if (curNode == nullptr) {
                curNode = &m_topNode;
            }
            level--;
            continue;
        }

        ctnt += c;
    }

    return true;
}

std::string CXMLParser::getContentText(const std::string& path, int index)
{
    SXMLNode* node = getNode(path);
    if (node == nullptr) {
        return "";
    }
    return node->getContentText(index);
}

bool CXMLParser::getContentText(const std::string& path, std::string& content, int index)
{
    SXMLNode* node = getNode(path);
    if (node == nullptr) {
        return false;
    }
    content = node->getContentText(index);
    return true;
}

std::string CXMLParser::getNodeName(char** in)
{
    std::string ret;
    char c;
    while ((c = **in) != 0) {
        if (c == ' ' || c == '\n' || c == '\r') {
            (*in)++;
            return ret;
        } else if ((c == '/' && *(*in + 1) == '>') || c == '>') {
            return ret;
        }
        ret += c;
        (*in)++;
    }

    return ret;
}

std::string CXMLParser::getAttrName(char** in)
{
    std::string ret;
    char c;

    while ((c = **in) != 0) {
        if (c == '=') {
            (*in)++;
            ret.erase(0, ret.find_first_not_of(" "));
            ret.erase(ret.find_last_not_of(" ") + 1);
            return ret;
        } else if ((c == '/' && *(*in + 1) == '>') || c == '>') {
            ret.erase(0, ret.find_first_not_of(" "));
            ret.erase(ret.find_last_not_of(" ") + 1);
            return ret;
        }
        ret += c;

        (*in)++;
    }

    ret.erase(0, ret.find_first_not_of(" "));
    ret.erase(ret.find_last_not_of(" ") + 1);
    return ret;
}

bool CXMLParser::getAttrValue(const char* name, char** in, std::string& strValue)
{
    char c;
    bool start = false;
    strValue = "";

    while ((c = **in) != 0) {
        if ((c == '/' && c == '>') || c == '>') {
            m_errorInfo = "Value of '";
            m_errorInfo = m_errorInfo + name + "' not ended with '\"'!";
            return false;
        } else if (c != ' ' && c != '"' && !start) {
            m_errorInfo = "Value of '";
            m_errorInfo = m_errorInfo + name + "' not started with '\"'!";
            return false;
        } else if (c == '"') {
            if (!start) {
                start = true;
                (*in)++;
                continue;
            }

            (*in)++;
            strValue.erase(0, strValue.find_first_not_of(" "));
            strValue.erase(strValue.find_last_not_of(" ") + 1);
            return true;
        }
        strValue += c;

        (*in)++;
    }
    return false;
}

std::string CXMLParser::getRemark(char** in)
{
    std::string ret;
    char c;

    while ((c = **in) != 0) {
        if (c == '-' && *((*in) + 1) == '>') {
            (*in) += 2;
            return ret;
        }
        ret += c;
        (*in)++;
    }

    return ret;
}

std::string CXMLParser::getCData(char** in)
{
    std::string ret;
    char c;

    while ((c = **in) != 0) {
        if (strncmp(*in, "]]>", 3) == 0) {
            (*in) += 3;
            return ret;
        }
        ret += c;
        (*in)++;
    }

    return ret;
}

std::string CXMLParser::getDocType(char** in)
{
    std::string ret;
    char c;

    while ((c = **in) != 0) {
        if (c == '>') {
            (*in) += 1;
            return ret;
        }
        ret += c;
        (*in)++;
    }

    return ret;
}

std::string CXMLParser::getFunction(char** in)
{
    std::string ret;
    char c;

    while ((c = **in) != 0) {
        if (c == '?' && *((*in) + 1) == '>') {
            (*in) += 2;
            return ret;
        }
        (*in)++;
    }
    return ret;
}

bool CXMLParser::getNodeAttrs(char** in, SXMLNode* node)
{
    char c = **in;
    while ((c = **in) != 0) {
        if (c == ' ' || c == '\n' || c == '\r') {
            (*in)++;
            continue;
        }

        if (c == '>') {
            (*in)++;
            return true;
        } else if (c == '/' && *(*in + 1) == '>') {
            return true;
        }

        SXMLNode::SXMLAttr* attr = new SXMLNode::SXMLAttr();
        if (attr == nullptr) {
            return false;
        }
        attr->m_strName = getAttrName(in);
        if (!getAttrValue(attr->m_strName.c_str(), in, attr->m_strValue)) {
            delete attr;
            return false;
        }

        node->addAttr(attr);
    }

    return true;
}
