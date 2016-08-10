// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
using System;
using System.Collections.Generic;
using System.Threading;
using Microsoft.Azure.IoT.Gateway;

namespace SensorModule
{
    public class DotNetCoreSensorModule : GatewayModule
    {
        private Thread _workerThread;

        protected override void Initialize( string configuration )
        {
            base.Initialize( configuration );
            this._workerThread = new Thread(this.ThreadBody);
            // Start the thread
            this._workerThread.Start();
        }

        protected override void Dispose( bool disposing )
        {
            Console.WriteLine("This is C# Sensor Module Destroy!");
            base.Dispose( disposing );
        }
 

        public void ThreadBody()
        {
            Random r = new Random();
            int n = r.Next();

            while (true)
            {
                Dictionary<string, string> thisIsMyProperty = new Dictionary<string, string>();
                thisIsMyProperty.Add("source", "sensor");

                Message messageToPublish = new Message("SensorData: " + n, thisIsMyProperty);

                this.Publish(messageToPublish);

                //Publish a message every 5 seconds. 
                Thread.Sleep(5000);
                n = r.Next();
            }
        }
    }
}
