using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace Microsoft.Azure.IoT.Gateway.Hosting
{
    /// <summary>Mediates between unmanged binding module interface and mangaed modules. This class is not intended to be called from application.</summary>
    public static class HostManager
    {
        private static readonly Dictionary<IntPtr, IGatewayModule> s_Modules = new Dictionary<IntPtr, IGatewayModule>();

#if DEBUG
        static HostManager()
        {
            Trace.Listeners.Add( new ConsoleTraceListener( useErrorStream: true ) );
        }
#endif // DEBUG

        private static void LogError( string message ) => Trace.TraceError( message );

        private static void LogError( string message, Exception ex ) => Trace.TraceError( "{0} {1}", message, ex );

        // For desktop .NET which does not support direct getting of delegate from unmanaged hosting API.
        /// <summary>Returns function pointer for specified method in this type. This method is not intended to be called from application.</summary>
        /// <param name="name">A name of the function.</param>
        /// <returns>A pointer to the API function.</returns>
        [AllowReversePInvokeCalls]
        public static unsafe long GetApi( string name )
        {
            try
            {
                if (name == null)
                    throw new ArgumentNullException(nameof(name));

                Delegate delg;
                switch (name)
                {
                    case "Create":
                        delg = new CreateApi(Create);
                        break;
                    case "Destroy":
                        delg = new ModuleApi(Destroy);
                        break;
                    case "Start":
                        delg = new ModuleApi( Start);
                        break;
                    case "Receive":
                        delg = new ReceiveApi(Receive);
                        break;
                    default:
                        throw new ArgumentException($"Unknown function {name}", nameof(name));
                }

                return Marshal.GetFunctionPointerForDelegate(delg).ToInt64();
            }
            catch (ArgumentException ex)
            {
                LogError("Invalid argument.", ex);
                throw;
            }
            catch (Exception ex)
            {
                LogError("Unexpected error.", ex);
                throw;
            }
        }
       
        /// <summary>Handles modules "Create" function. This method is not intended to be called from application.</summary>
        /// <param name="brokerHandle">A handle to a broker.</param>
        /// <param name="moduleHandle">A handle to a module.</param>
        /// <param name="assemblyNameUtf8">A pointer to null-terminated utf-8 string which represents module assembly name.</param>
        /// <param name="moduleTypeNameUtf8">A pointer to null-terminated utf-8 string which represents module type full name.</param>
        /// <param name="configuration">A pointer to null-terminated utf-8 string which represents JSON for module initialization arguments.</param>
        /// <returns><see cref="HostManagerResult"/> to distinguish invocation result.</returns>
        [AllowReversePInvokeCalls]
        public static unsafe HostManagerResult Create(
            IntPtr brokerHandle,
            IntPtr moduleHandle,
            byte* assemblyNameUtf8,
            byte* moduleTypeNameUtf8,
            byte* configuration)
        {
            // Do not use closure here to avoid heap allocation as possible.
            try
            {
                string assemblyName = ToUtf16String(assemblyNameUtf8);
                string moduleTypeName = ToUtf16String(moduleTypeNameUtf8);
                var broker = new Broker(brokerHandle.ToInt64(), moduleHandle.ToInt64());

                if (s_Modules.ContainsKey(moduleHandle))
                {
                    LogError($"Module 0x{moduleHandle.ToInt64():X} is already used.");
                    return HostManagerResult.InvalidArgument;
                }

                Assembly assembly;
                try
                {
                    assembly = Assembly.Load(assemblyName);
                }
                catch (Exception ex)
                {
                    LogError($"Failed to load assembly {assemblyName}.", ex);
                    return HostManagerResult.AssemblyLoadFail;
                }

                Type moduleType;
                try
                {
                    moduleType = assembly.GetType(moduleTypeName, throwOnError: true);
                }
                catch (Exception ex)
                {
                    LogError($"Failed to load module type {moduleTypeName}.", ex);
                    return HostManagerResult.TypeLoadFail;
                }

                object moduleObject;
                try
                {
                    moduleObject = Activator.CreateInstance(moduleType);
                }
                catch (Exception ex)
                {
                    LogError($"Failed to instantiate module {moduleTypeName}.", ex);
                    return HostManagerResult.ModuleInstatiationFail;
                }

                var module = moduleObject as IGatewayModule;
                if (module == null)
                {
                    LogError($"Module {moduleTypeName} does not implement IGatewayModule.");
                    return HostManagerResult.InvalidModuleType;
                }

                var configurationBytes = ToByteArray(configuration);
                fixed (byte* pConfigurationBytes = configurationBytes)
                {
                    for (int i = 0; i < configurationBytes.Length; i++)
                        pConfigurationBytes[i] = configuration[i];
                }

                try
                {
                    module.Create(broker, configurationBytes);
                }
                catch (Exception ex)
                {
                    LogError($"{moduleTypeName}.Create failed.", ex);
                    return HostManagerResult.ModuleFail;
                }

                s_Modules[moduleHandle] = module;

                return HostManagerResult.Success;
            }
            catch (ArgumentException ex)
            {
                LogError("Invalid argument.", ex);
                return HostManagerResult.InvalidArgument;
            }
            catch (Exception ex)
            {
                LogError("Unexpected error.", ex);
                return HostManagerResult.UnexpectedError;
            }
        }

        /// <summary>Handles modules "Receive" function. This method is not intended to be called from application.</summary>
        /// <param name="moduleHandle">A handle to a module.</param>
        /// <param name="messageBytes">A pointer to message bytes including properties.</param>
        /// <param name="messageLength">A length of the <paramref name="messageBytes"/> bytes.</param>
        /// <returns><see cref="HostManagerResult"/> to distinguish invocation result.</returns>
        [AllowReversePInvokeCalls]
        public static unsafe HostManagerResult Receive(
            IntPtr moduleHandle,
            byte* messageBytes,
            int messageLength)
        {
            // Do not use closure here to avoid heap allocation as possible.
            try
            {
                if (moduleHandle == IntPtr.Zero)
                    throw new ArgumentNullException(nameof(moduleHandle));

                if (messageBytes == null)
                    throw new ArgumentNullException(nameof(messageBytes));

                if (messageLength < 0)
                    throw new ArgumentOutOfRangeException(nameof(messageLength));

                IGatewayModule module;
                if (!s_Modules.TryGetValue(moduleHandle, out module))
                    return HostManagerResult.InvalidArgument;

                /* Codes_SRS_DOTNET_04_017: [ DotNet_Receive shall construct an instance of the Message interface as defined below: ] */
                /* Codes_SRS_DOTNET_04_018: [ DotNet_Receive shall call Receive C# method passing the Message object created with the content of message serialized into Message object. ] */
                var message = new Message(ToByteArray(messageBytes, messageLength));

                try
                {
                    module.Receive(message);
                    return HostManagerResult.Success;
                }
                catch (Exception ex)
                {
                    LogError("Unexpected exception in IGatewayModule.Receive.", ex);
                    return HostManagerResult.ModuleFail;
                }
            }
            catch (ArgumentException ex)
            {
                LogError("Invalid argument.", ex);
                return HostManagerResult.InvalidArgument;
            }
            catch ( Exception ex )
            {
                LogError("Unexpected error.", ex);
                return HostManagerResult.UnexpectedError;
            }
        }

        /// <summary>Handles modules "Destroy" function. This method is not intended to be called from application.</summary>
        /// <param name="moduleHandle">A handle to a module.</param>
        /// <returns><see cref="HostManagerResult"/> to distinguish invocation result.</returns>
        [AllowReversePInvokeCalls]
        public static HostManagerResult Destroy(IntPtr moduleHandle)
        {
            try
            {
                if (moduleHandle == IntPtr.Zero)
                    throw new ArgumentNullException(nameof(moduleHandle));

                IGatewayModule module;
                if (!s_Modules.TryGetValue(moduleHandle, out module))
                    return HostManagerResult.InvalidArgument;

                try
                {
                    module.Destroy();
                }
                catch (Exception ex)
                {
                    LogError("Unexpected exception in IGatewayModule.Destroy.", ex);
                    return HostManagerResult.ModuleFail;
                }

                s_Modules.Remove(moduleHandle);
                return HostManagerResult.Success;
            }
            catch (ArgumentException ex)
            {
                LogError("Invalid argument.", ex);
                return HostManagerResult.InvalidArgument;
            }
            catch (Exception ex)
            {
                LogError("Unexpected error.", ex);
                return HostManagerResult.UnexpectedError;
            }
        }

        /// <summary>Handles modules "Start" function. This method is not intended to be called from application.</summary>
        /// <param name="moduleHandle">A handle to a module.</param>
        /// <returns><see cref="HostManagerResult"/> to distinguish invocation result.</returns>
        [AllowReversePInvokeCalls]
        public static HostManagerResult Start(IntPtr moduleHandle)
        {
            try
            {
                if (moduleHandle == IntPtr.Zero)
                    throw new ArgumentNullException(nameof(moduleHandle));

                IGatewayModule module;
                if (!s_Modules.TryGetValue(moduleHandle, out module))
                    return HostManagerResult.InvalidArgument;

                try
                {
                    var asStart = module as IGatewayModuleStart;
                    if (asStart != null)
                        asStart.Start();

                    return HostManagerResult.Success;
                }
                catch (Exception ex)
                {
                    LogError("Unexpected exception in IGatewayModuleStart.Start.", ex);
                    return HostManagerResult.ModuleFail;
                }
            }
            catch (ArgumentException ex)
            {
                LogError("Invalid argument.", ex);
                return HostManagerResult.InvalidArgument;
            }
            catch (Exception ex)
            {
                LogError("Unexpected error.", ex);
                return HostManagerResult.UnexpectedError;
            }
        }

        private static unsafe int GetLength(byte* utf8NullTerminated)
        {
            int length = 0;
            while (true)
            {
                if (utf8NullTerminated[ length ] == 0)
                    return length;

                length++;
            }
        }

        private static unsafe string ToUtf16String(byte* utf8NullTerminated)
            => Encoding.UTF8.GetString(ToByteArray(utf8NullTerminated));

        private static unsafe byte[] ToByteArray(byte* utf8NullTerminated)
            => ToByteArray(utf8NullTerminated, GetLength(utf8NullTerminated));

        private static unsafe byte[] ToByteArray(byte* utf8NullTerminated, int length)
        {
            var value = new byte[length];
            Marshal.Copy(new IntPtr(utf8NullTerminated), value, 0, length);
            return value;
        }

        private unsafe delegate HostManagerResult CreateApi(IntPtr broker, IntPtr module, byte* assemblyName, byte* typeName, byte* configuration);

        private unsafe delegate HostManagerResult ReceiveApi(IntPtr module, byte* message, int messageLength);
        
        // This is required because Marshal.GetFunctionPtrForDelegate requires non-generic delegate.
        private delegate HostManagerResult ModuleApi(IntPtr module);

    }
}