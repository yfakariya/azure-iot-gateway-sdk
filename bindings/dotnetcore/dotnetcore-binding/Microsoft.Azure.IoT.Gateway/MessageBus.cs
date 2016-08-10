// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

using System;

namespace Microsoft.Azure.IoT.Gateway
{
    /// <summary> Object that represents the bus, to where a messsage is going to be published </summary>
    internal struct MessageBus
    {
        private readonly IntPtr _messageBusHandle;

        private readonly IntPtr _moduleHandle;
        
        public MessageBus(IntPtr moduleHandle, IntPtr messageBusHandle)
        {
            this._messageBusHandle = messageBusHandle;
            this._moduleHandle = moduleHandle;
        }

        /// <summary>
        ///     Publish a message into the gateway message bus. 
        /// </summary>
        /// <param name="message">Object representing the message to be published into the bus.</param>
        /// <returns></returns>
        public void Publish(Message message)
        {
            if ( this._messageBusHandle == IntPtr.Zero )
            {
                throw new InvalidOperationException( "MessageBus is not initialized yet." );
            }

            /* Codes_SRS_DOTNET_MESSAGEBUS_04_004: [ Publish shall not catch exception thrown by ToByteArray. ] */
            /* Codes_SRS_DOTNET_MESSAGEBUS_04_003: [ Publish shall call the Message.ToByteArray() method to get the Message object translated to byte array. ] */
            byte[] messageObject = message.ToByteArray();

            /* Codes_SRS_DOTNET_MESSAGEBUS_04_005: [ Publish shall call the native method Module_DotNetHost_PublishMessage passing the msgBus and moduleHandle value saved by it's constructor, the byte[] got from Message and the size of the byte array. ] */
            NativeMethods.PublishMessage(this._messageBusHandle, this._moduleHandle, messageObject, messageObject.Length);
        }
    }
}

