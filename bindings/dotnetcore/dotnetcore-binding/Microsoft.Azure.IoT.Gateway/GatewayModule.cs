// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

using System;

namespace Microsoft.Azure.IoT.Gateway
{
    /// <summary> Interface to be implemented by the .NET Module </summary>
    public abstract class GatewayModule : IDisposable
    {
        private MessageBus _messageBus;

        protected GatewayModule() { }

        public void Dispose()
        {
            this.Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose( bool disposing )
        {
            // nop
        }

        /// <summary>
        ///     Creates a module using the specified configuration connecting to the specified message bus.
        /// </summary>
        /// <param name="bus">The bus onto which this module will connect.</param>
        /// <param name="configuration">A string with user-defined configuration for this module.</param>
        /// <returns></returns>
        internal void Create( MessageBus bus, string configuration )
        {
            this.Initialize( configuration );
        }

        protected virtual void Initialize( string configuration )
        {
            // nop;
        }

        protected virtual void Publish( Message message )
        {
            this._messageBus.Publish( message );
        }

        /// <summary>
        ///     The module's callback function that is called upon message receipt.
        /// </summary>
        /// <param name="receivedMessage">The message being sent to the module.</param>
        /// <returns></returns>                
        internal void Receive( Message receivedMessage )
        {
            this.OnReceived( receivedMessage );
        }

        protected virtual void OnReceived( Message receivedMessage )
        {
            // nop
        }
    }
}
