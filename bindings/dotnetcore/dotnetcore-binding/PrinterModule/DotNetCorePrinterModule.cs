// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
using System;
using System.Text;
using Microsoft.Azure.IoT.Gateway;


namespace PrinterModule
{
    public class DotNetCorePrinterModule : GatewayModule
    {
        protected override void Dispose(bool disposing)
        {
            Console.WriteLine("This is C# Sensor Module Destroy!");
            base.Dispose(disposing);
        }


        protected override void OnReceived(Message receivedMessage)
        {
            if (receivedMessage.Properties["source"] == "sensor")
            {
                Console.WriteLine("Printer Module received message from Sensor. Content: " + Encoding.UTF8.GetString(receivedMessage.Content, 0, receivedMessage.Content.Length));
            }
        }
    }
}
