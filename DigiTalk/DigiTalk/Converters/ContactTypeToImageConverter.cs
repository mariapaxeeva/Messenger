// Конвертер для иконки контакта
// Если контакт - пользователь, конвертер возвращает аватарку пользователя, иначе - аватарку группового чата

using Avalonia.Data.Converters;
using System.Globalization;
using System;
using Avalonia.Media.Imaging;
using Avalonia.Platform;

namespace DigiTalk.Converters
{
    public class ContactTypeToImageConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool isGroup)
            {
                try
                {
                    // Путь к изображению
                    var uri = isGroup
                        ? "avares://DigiTalk/Assets/icons/group-avatar.png"
                        : "avares://DigiTalk/Assets/icons/user-avatar.png";

                    // Загрузка изображения
                    var bitmap = new Bitmap(AssetLoader.Open(new Uri(uri)));
                    return bitmap;
                }
                catch (Exception ex)
                {
                    return null!;
                }
            }
            return null!;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}