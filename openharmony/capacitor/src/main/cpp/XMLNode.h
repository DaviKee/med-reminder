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

#ifndef XML_NODE_H
#define XML_NODE_H

#include <string>

/**
 * @brief Defines the structure of XML.
 */
struct SXMLDocElement {
    enum SXMLDocElementType { XML_ELM_UNKNOWN = 0, XML_ELM_NODE, XML_ELM_CONTENT };

    SXMLDocElementType m_type;
    SXMLDocElement* m_next;
    SXMLDocElement* m_parent;
    int m_nLevel;

    SXMLDocElement()
    {
        m_type = XML_ELM_UNKNOWN;
        m_next = nullptr;
        m_parent = nullptr;
        m_nLevel = 0;
    }
};

struct SXMLDocument : public SXMLDocElement {
    std::string m_strContent;

    SXMLDocument() : SXMLDocElement()
    {
        m_type = XML_ELM_CONTENT;
    }
};

struct SXMLNode : public SXMLDocElement {
    struct SXMLAttr {
        std::string m_strName;
        std::string m_strValue;
        SXMLAttr* m_pNext;

        SXMLAttr()
        {
            m_pNext = nullptr;
        }
    };

    std::string m_strName;

    SXMLAttr* m_pAttrs;
    SXMLDocElement* m_pElements;

    SXMLNode() : SXMLDocElement()
    {
        m_type = XML_ELM_NODE;
        m_pElements = nullptr;
        m_pAttrs = nullptr;
    }

    ~SXMLNode();

    /**
     * @brief Releases all child elements
     */
    void freeChildElements();

    /**
     * @brief Releases all child nodes
     */
    void freeChildAttrs();

    /**
     * @brief Gets the raw content
     * @return std::string: Raw content std::string
     */
    std::string getRawContent();

    /**
     * @brief Adds an element to the node
     * @param element:The element to be added
     * @return bool: true for success or false failed.
     */
    bool addElement(SXMLDocElement* element);

    /**
     * @brief Adds a new node
     * @param node:The node to be added
     * @return bool: true for success or false failed.
     */
    bool addNode(SXMLNode* node);

    /**
     * @brief Adds an attribute to the node
     * @param attr: attribute object will be added.
     * @return bool: true for success or false failed.
     */
    bool addAttr(SXMLAttr* attr);

    /**
     * @brief Gets the number of elements
     * @param type:type of the element to be searched.
     * @return int: number of children
     */
    int getElementsCount(SXMLDocElementType type = XML_ELM_UNKNOWN);

    /**
     * @brief Gets the number of nodes
     * @return int: number of children
     */
    int getNodesCount();

    /**
     * @brief get element by index.
     * @param type:type of the element to be searched.
     * @param index:position index of the element to be searched.
     * @return SXMLDocElement: element point or nullptr if failed.
     */
    SXMLDocElement* getDocElement(SXMLDocElementType type = XML_ELM_UNKNOWN, int index = 0);

    /**
     * @brief get number of attributes
     * @return int - number of attributes
     */
    int getAttrsCount();

    /**
     * @brief get node by path
     * @param path：name of the sub node to be searched.
     * @return SXMLNode*: node point or nullptr if failed.
     */
    SXMLNode* getNode(const char* path);

    /**
     * @brief get node by name
     * @param name：name of the sub node to be searched.
     * @return SXMLNode*: node point or nullptr if failed.
     */
    SXMLNode* getNodeStep(const char* name);

    /**
     * @brief get node by index.
     * @param index: position index of the node to be searched.
     * @return SXMLNode*:  node point or nullptr if failed.
     */
    SXMLNode* getNode(int nIndex = 0);

    /**
     * @brief get content text by index.
     * @param index: position index of the content to be searched.
     * @return std::string: text in this content.
     */
    std::string getContentText(int nIndex = 0);

    /**
     * @brief get content text by index.
     * @param index: position index of the content to be searched.
     * @return std::string: text in this content.
     */
    bool getContentText(std::string& text, int nIndex = 0);

    /**
     * @brief get content by index.
     * @param index: position index of the content to be searched.
     * @return SXMLContent*: content point or nullptr if failed.
     */
    SXMLDocument* getContent(int nIndex = 0);

    /**
     * @brief get attribute by name.
     * @param name: name of the attribute to be searched.
     * @return SXMLAttr*: node's attribute or nullptr if failed.
     */
    SXMLAttr* getAttr(const char* name);

    /**
     * @brief get attribute by index.
     * @param index: position index of the content to be searched.
     * @return SXMLAttr*: node's attribute or nullptr if failed.
     */
    SXMLAttr* getAttr(int nIndex);

    /**
     * @brief get attribute by name.
     * @param name: name of the attribute to be searched.
     * @param value: value of the attribute to be searched.
     * @return bool: true if get node's attribute value success or false if failed.
     */
    bool getAttrValue(const char* name, std::string& value);
};

#endif
