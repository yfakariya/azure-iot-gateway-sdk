// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace Microsoft.Azure.IoT.Gateway
{
    /// <summary> Object that represents a message on the message bus. </summary>
    public sealed class Message
    {
        /// <summary>
        ///   Message Content.
        /// </summary>
        public byte[] Content { get; }

        private readonly Dictionary<string, string> _properties;

        /// <summary>
        ///    Message Properties.
        /// </summary>
        public IDictionary<string, string> Properties { get { return this._properties; } }

        /// <summary>
        ///     Constructor for Message. This receives a byte array. Format defined at <a href="https://github.com/Azure/azure-iot-gateway-sdk/blob/master/core/devdoc/message_requirements.md">message_requirements.md</a>.
        /// </summary>
        /// <param name="messageAsByteArray">ByteArray with the Content and Properties of a message.</param>
        public unsafe Message(byte[] messageAsByteArray)
        {
            if (messageAsByteArray == null)
            {
                /* Codes_SRS_DOTNET_MESSAGE_04_008: [ If any parameter is null, constructor shall throw a ArgumentNullException ] */
                throw new ArgumentNullException(nameof(messageAsByteArray), "messageAsByteArray cannot be null");
            }

            this._properties = new Dictionary<string, string>();
            fixed (byte* pMessage = messageAsByteArray)
            {
                this.Content = ReadMessage(pMessage, messageAsByteArray.Length, this._properties);
            }
        }

        internal unsafe Message(byte* messageAsByteArray, int length)
        {
            this._properties = new Dictionary<string, string>();
            this.Content = ReadMessage(messageAsByteArray, length, this._properties);
        }

        private static unsafe byte[] ReadMessage(byte* messageAsByteArray, int length, IDictionary<string, string> properties)
        {
            if (messageAsByteArray == null)
            {
                /* Codes_SRS_DOTNET_MESSAGE_04_008: [ If any parameter is null, constructor shall throw a ArgumentNullException ] */
                throw new ArgumentNullException(nameof(messageAsByteArray), "messageAsByteArray cannot be null");
            }

            byte* headOfMessage = messageAsByteArray;

            /* Codes_SRS_DOTNET_MESSAGE_04_002: [ Message class shall have a constructor that receives a byte array with it's content format as described in message_requirements.md and it's Content and Properties are extracted and saved. ] */
            if (length >= 14)
            {
                byte header1 = *messageAsByteArray;
                messageAsByteArray++;
                byte header2 = *messageAsByteArray;
                messageAsByteArray++;

                if (header1 == (byte)0xA1 && header2 == (byte)0x60)
                {
                    int arraySizeInInt;
                    try
                    {
                        arraySizeInInt = ReadInt32(messageAsByteArray, length - (messageAsByteArray - headOfMessage));
                        messageAsByteArray += sizeof(int);
                    }
                    catch (ArgumentException e)
                    {
                        /* Codes_SRS_DOTNET_MESSAGE_04_006: [ If byte array received as a parameter to the Message(byte[] msgInByteArray) constructor is not in a valid format, it shall throw an ArgumentException ] */
                        throw new ArgumentException("Could not read array size information.", e);
                    }

                    if (arraySizeInInt >= int.MaxValue)
                    {
                        /* Codes_SRS_DOTNET_MESSAGE_04_006: [ If byte array received as a parameter to the Message(byte[] msgInByteArray) constructor is not in a valid format, it shall throw an ArgumentException ] */
                        throw new ArgumentException("Size of MsgArray can't be more than MAXINT.");
                    }

                    if (length != arraySizeInInt)
                    {
                        /* Codes_SRS_DOTNET_MESSAGE_04_006: [ If byte array received as a parameter to the Message(byte[] msgInByteArray) constructor is not in a valid format, it shall throw an ArgumentException ] */
                        throw new ArgumentException("Array Size information doesn't match with array size.");
                    }

                    int propertyCount;

                    try
                    {
                        propertyCount = ReadInt32(messageAsByteArray, length - (messageAsByteArray - headOfMessage));
                        messageAsByteArray += sizeof(int);
                    }
                    catch (ArgumentException e)
                    {
                        /* Codes_SRS_DOTNET_MESSAGE_04_006: [ If byte array received as a parameter to the Message(byte[] msgInByteArray) constructor is not in a valid format, it shall throw an ArgumentException ] */
                        throw new ArgumentException("Could not read property count.", e);
                    }

                    if (propertyCount < 0)
                    {
                        /* Codes_SRS_DOTNET_MESSAGE_04_006: [ If byte array received as a parameter to the Message(byte[] msgInByteArray) constructor is not in a valid format, it shall throw an ArgumentException ] */
                        throw new ArgumentException("Number of properties can't be negative.");
                    }

                    if (propertyCount >= int.MaxValue)
                    {
                        /* Codes_SRS_DOTNET_MESSAGE_04_006: [ If byte array received as a parameter to the Message(byte[] msgInByteArray) constructor is not in a valid format, it shall throw an ArgumentException ] */
                        throw new ArgumentException("Number of properties can't be more than MAXINT.");
                    }

                    if (propertyCount > 0)
                    {
                        //Here is where we are going to read the properties.
                        for (int count = 0; count < propertyCount; count++)
                        {
                            string key, value;
                            try
                            {
                                int progress;
                                key = ReadNullTerminatedUtf8String(messageAsByteArray, length - (messageAsByteArray - headOfMessage), out progress);
                                messageAsByteArray += progress;
                            }
                            catch (Exception ex)
                            {
                                /* Codes_SRS_DOTNET_MESSAGE_04_006: [ If byte array received as a parameter to the Message(byte[] msgInByteArray) constructor is not in a valid format, it shall throw an ArgumentException ] */
                                throw new ArgumentException("Could not parse Properties(key)", ex);
                            }
                            try
                            {
                                int progress;
                                value = ReadNullTerminatedUtf8String(messageAsByteArray, length - (messageAsByteArray - headOfMessage), out progress);
                                messageAsByteArray += progress;
                            }
                            catch (Exception ex)
                            {
                                /* Codes_SRS_DOTNET_MESSAGE_04_006: [ If byte array received as a parameter to the Message(byte[] msgInByteArray) constructor is not in a valid format, it shall throw an ArgumentException ] */
                                throw new ArgumentException("Could not parse Properties(key)", ex);
                            }

                            properties.Add(key, value);
                        }
                    }

                    long contentLengthPosition = (headOfMessage - messageAsByteArray);
                    int contentLength;

                    try
                    {
                        contentLength = ReadInt32(messageAsByteArray, length - (messageAsByteArray - headOfMessage));
                    }
                    catch (ArgumentException e)
                    {
                        /* Codes_SRS_DOTNET_MESSAGE_04_006: [ If byte array received as a parameter to the Message(byte[] msgInByteArray) constructor is not in a valid format, it shall throw an ArgumentException ] */
                        throw new ArgumentException("Could not read contentLength.", e);
                    }


                    // Verify if the number of content matches with the real number of content. 
                    // 4 is the number of bytes that describes the contentLengthPosition information.
                    if (arraySizeInInt - contentLengthPosition - 4 != contentLength)
                    {
                        /* Codes_SRS_DOTNET_MESSAGE_04_006: [ If byte array received as a parameter to the Message(byte[] msgInByteArray) constructor is not in a valid format, it shall throw an ArgumentException ] */
                        throw new ArgumentException("Size of byte array doesn't match with current content.");
                    }

                    byte[] content = new byte[contentLength];
                    fixed (byte* pContent = content)
                    {
                        Buffer.MemoryCopy(messageAsByteArray, pContent, contentLength, contentLength);
                    }

                    return content;
                }
                else
                {
                    /* Codes_SRS_DOTNET_MESSAGE_04_006: [ If byte array received as a parameter to the Message(byte[] msgInByteArray) constructor is not in a valid format, it shall throw an ArgumentException ] */
                    throw new ArgumentException("Invalid Header bytes.");
                }
            }
            else
            {
                /* Codes_SRS_DOTNET_MESSAGE_04_006: [ If byte array received as a parameter to the Message(byte[] msgInByteArray) constructor is not in a valid format, it shall throw an ArgumentException ] */
                throw new ArgumentException("Invalid byte array size.");
            }
        }

        private static unsafe string ReadNullTerminatedUtf8String(byte* bytes, long remains, out int actualLength)
        {
            int length = 0;
            bool found = false;
            for (; length < remains; length++)
            {
                if (bytes[length] == 0)
                {
                    found = true;
                    break;
                }

                if (bytes[length] == 255)
                {
                    throw new FormatException("Invalid UTF-8 sequence.");
                }
            }

            if (!found)
            {
                throw new ArgumentException("A string is not terminated.");
            }

            actualLength = length;
            return Encoding.UTF8.GetString(bytes, length);
        }

        private static unsafe int ReadInt32(byte* stream, long remains)
        {
            if (remains < sizeof(int))
            {
                throw new ArgumentException("Failed to read Int32 value from stream.");
            }

            int result = 0;

            if (BitConverter.IsLittleEndian)
            {
                byte* pResult = (byte*)&result + sizeof(int);
                *(--pResult) = *stream;
                *(--pResult) = *(++stream);
                *(--pResult) = *(++stream);
                *(--pResult) = *(++stream);
            }
            else
            {
                byte* pResult = (byte*)&result;
                *pResult = *stream;
                *(++pResult) = *(++stream);
                *(++pResult) = *(++stream);
                *(++pResult) = *(++stream);
            }

            return result;
        }
        /// <summary>
        ///     Constructor for Message. This constructor receives a byte[] as it's content and Properties.
        /// </summary>
        /// <param name="contentAsByteArray">Content of the Message</param>
        /// <param name="properties">Set of Properties that will be added to a message.</param>
        public Message(byte[] contentAsByteArray, IDictionary<string, string> properties)
        {
            if (contentAsByteArray == null)
            {
                /* Codes_SRS_DOTNET_MESSAGE_04_008: [ If any parameter is null, constructor shall throw a ArgumentNullException ] */
                throw new ArgumentNullException("contentAsByteArray", "contentAsByteArray cannot be null");
            }

            if (properties == null)
            {
                /* Codes_SRS_DOTNET_MESSAGE_04_008: [ If any parameter is null, constructor shall throw a ArgumentNullException ] */
                throw new ArgumentNullException("properties", "properties cannot be null");
            }

            /* Codes_SRS_DOTNET_MESSAGE_04_004: [ Message class shall have a constructor that receives a content as byte[] and properties, storing them. ] */
            this.Content = contentAsByteArray;
            this._properties = properties as Dictionary<string, string> ?? new Dictionary<string, string>(properties);
        }

        /// <summary>
        ///     Constructor for Message. This constructor receives a string as it's content and Properties.
        /// </summary>
        /// <param name="content">String with the ByteArray with the Content and Properties of a message.</param>
        /// <param name="properties">Set of Properties that will be added to a message.</param>
        public Message(string content, IDictionary<string, string> properties)
        {

            if (content == null)
            {
                /* Codes_SRS_DOTNET_MESSAGE_04_008: [ If any parameter is null, constructor shall throw a ArgumentNullException ] */
                throw new ArgumentNullException("content", "content cannot be null");
            }
            if (properties == null)
            {
                /* Codes_SRS_DOTNET_MESSAGE_04_008: [ If any parameter is null, constructor shall throw a ArgumentNullException ] */
                throw new ArgumentNullException("properties", "properties cannot be null");
            }

            /* Codes_SRS_DOTNET_MESSAGE_04_003: [ Message class shall have a constructor that receives a content as string and properties and store it. This string shall be converted to byte array based on System.Text.Encoding.UTF8.GetBytes(). ] */
            this.Content = Encoding.UTF8.GetBytes(content);
            this._properties = properties as Dictionary<string, string> ?? new Dictionary<string, string>(properties);
        }

        /// <summary>
        ///     Constructor for Message. This constructor receives another Message as a parameter.
        /// </summary>
        /// <param name="message">Message Instance.</param>
        public Message(Message message)
        {
            if (message == null)
            {
                /* Codes_SRS_DOTNET_MESSAGE_04_008: [ If any parameter is null, constructor shall throw a ArgumentNullException ] */
                throw new ArgumentNullException(nameof(message));
            }

            this.Content = message.Content;
            this._properties = message._properties;
        }

        /// <summary>
        ///    Converts the message into a byte array (Format defined at <a href="https://github.com/Azure/azure-iot-gateway-sdk/blob/master/core/devdoc/message_requirements.md">message_requirements.md</a>).
        /// </summary>
        public byte[] ToByteArray()
        {
            // TODO: avoid allocation if necessary / available.

            /* Codes_SRS_DOTNET_MESSAGE_04_005: [ Message Class shall have a ToByteArray method which will convert it's byte array Content and it's Properties to a byte[] which format is described at message_requirements.md ] */

            using (var buffer = new MemoryStream())
            {
                // Fill the first 2 bytes with 0xA1 and 0x60
                buffer.WriteByte(0xA1);
                buffer.WriteByte(0x60);

                // Skip 4 bytes for future array size storage.
                buffer.Position += 4;

                // Fill the 4 bytes with the amount of properties;
                WriteInt32(buffer, this._properties.Count);

                // Fill the bytes with content from key/value of properties (null terminated string separated);
                foreach (var property in this._properties)
                {
                    byte[] key = Encoding.UTF8.GetBytes(property.Key);
                    buffer.Write(key, 0, key.Length);

                    byte[] value = Encoding.UTF8.GetBytes(property.Value);
                    buffer.Write(value, 0, value.Length);
                }

                // Fill the amount of bytes on the content in 4 bytes after the properties; 
                WriteInt32(buffer, this.Content.Length);

                // Fill up the bytes with the message content. 
                buffer.Write(this.Content, 0, this.Content.Length);

                // Fill the 4 bytes with the array size;
                buffer.Position = 2;
                WriteInt32(buffer, (int)buffer.Length);

                return buffer.ToArray();
            }
        }
        private static void WriteInt32(Stream stream, int value)
        {
            byte[] size = BitConverter.GetBytes(value);
            if (BitConverter.IsLittleEndian)
            {
                Array.Reverse(size);
            }

            stream.Write(size, 0, size.Length);
        }
    }
}
